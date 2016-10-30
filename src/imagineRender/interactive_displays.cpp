#include "imagine_render.h"

using namespace Imagine;

#define SEND_ALL_FRAMES 1

// splitting the functions from the same class into separate files is a nasty thing to do, but 
// there's so much shared state that for the moment, it's a lot easier...
// TODO: This stuff in here can be encapsulated in a class quite easily...

bool ImagineRender::setupPreviewDataChannel(Foundry::Katana::Render::RenderSettings& settings)
{
	m_rawKatanaHost = getKatanaHost();

	size_t portSep = m_rawKatanaHost.find(":");

	if (portSep == std::string::npos)
	{
		fprintf(stderr, "Error: Katana host and port not specified...\n");
		return false;
	}

	m_katanaHost = m_rawKatanaHost.substr(0, portSep);

	std::string strPort = m_rawKatanaHost.substr(portSep + 1);
	m_katanaPort = atol(strPort.c_str());

	// perversely, Katana requires the port number retrieved above to be incremented by 100,
	// despite the fact that KatanaPipe::connect() seems to handle doing that itself for the control
	// port (which is +100 the data port). Not doing this causes Katana to segfault on the connect()
	// call, leaving renderBoot orphaned doing the render in the background...
	m_katanaPort += 100;

	// customise channels we're going to do from settings...
	bool doExtaChannels = true;

	//
#if ENABLE_PREVIEW_RENDERS
	FnKat::Render::RenderSettings::ChannelBuffers interactiveRenderBuffers;
	settings.getChannelBuffers(interactiveRenderBuffers);

	FnKat::Render::RenderSettings::ChannelBuffers::const_iterator itBuffer = interactiveRenderBuffers.begin();
	for (; itBuffer != interactiveRenderBuffers.end(); ++itBuffer)
	{
		const FnKat::Render::RenderSettings::ChannelBuffer& buffer = (*itBuffer).second;

		const std::string& channelName = (*itBuffer).first;
		m_aInteractiveChannelNames.push_back(channelName);
		
		int frameBufferID = atoi(buffer.bufferId.c_str());
		m_aInteractiveFrameIDs.push_back(frameBufferID);

		std::string channelType = buffer.channelName;

		// detect extra AOVs...

		if (doExtaChannels && channelName == "n")
		{
			channelType = "rgb";
			m_aExtraInteractiveAOVs.push_back(RenderAOV(channelName, channelType, 3, 3, frameBufferID));
			m_renderSettings.add("output_normals", true);
			m_extraAOVsFlags |= COMPONENT_NORMAL;
		}
		else if (doExtaChannels && channelName == "wpp")
		{
			m_renderSettings.add("output_wpp", true);
			m_extraAOVsFlags |= COMPONENT_WPP;
		}

//		fprintf(stderr, "Buffers: %s: %i, %s\n", channelName.c_str(), frameBufferID, channelType.c_str());
	}
	
	if (m_enableIDPicking)
	{
		// Katana doesn't include ID channel in results from getChannelBuffers() so we have to just add it...
		
		m_aInteractiveChannelNames.push_back("__id");
		
		int frameBufferID = m_aInteractiveFrameIDs[0];
		m_aInteractiveFrameIDs.push_back(frameBufferID);

		std::string channelName = "__id";
		std::string channelType = "id";
		
		m_aExtraInteractiveAOVs.push_back(RenderAOV(channelName, channelType, 1, 4, frameBufferID));
		m_extraAOVsFlags |= COMPONENT_ID;

		m_renderSettings.add("output_ids", true);		
	}

	if (m_aInteractiveFrameIDs.empty())
	{
		fprintf(stderr, "Error: no channel buffers specified for interactive rendering...\n");
		return false;
	}

	// Katana expects the frame to be formatted like this, based on the ID pass frame...
	// Without doing this, it can't identify frames and channels...
	char szFN[128];
	sprintf(szFN, "id_katana_%i", m_aInteractiveFrameIDs[0]);
	std::string fFrameName(szFN);

	//

	if (m_pDataPipe)
	{
		delete m_pDataPipe;
		m_pDataPipe = NULL;
	}

	m_pDataPipe = FnKat::PipeSingleton::Instance(m_katanaHost, m_katanaPort);

	// try and connect...
	if (m_pDataPipe->connect() != 0)
	{
		fprintf(stderr, "Error: Can't connect to Katana data socket: %s:%u\n", m_katanaHost.c_str(), m_katanaPort);
		return false;
	}

	unsigned int originX = 0;
	unsigned int originY = 0;
/*
	if (m_ROIActive)
	{
		originX += m_ROIStartX;
		originY += m_ROIStartY;
	}
*/

	if (m_pFrame)
	{
		delete m_pFrame;
		m_pFrame = NULL;
	}

	// technically leaks, but...
	m_pFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);

	int localFrameBufferID = m_aInteractiveFrameIDs[0];

	// set the name
	std::string frameName;
	FnKat::encodeLegacyName(fFrameName, localFrameBufferID, frameName);
	m_pFrame->setFrameName(frameName);

	m_pDataPipe->send(*m_pFrame);

	// build up channels we're going to do - always do the primary

	if (m_pPrimaryChannel)
	{
		delete m_pPrimaryChannel;
		m_pPrimaryChannel = NULL;
	}

	int channelID = 0;
	// technically leaks, but...
	m_pPrimaryChannel = new FnKat::NewChannelMessage(*m_pFrame, channelID, m_renderHeight, m_renderWidth, originX, originY, 1.0f, 1.0f);

	std::string channelName;
	FnKat::encodeLegacyName(fFrameName, localFrameBufferID, channelName);
	m_pPrimaryChannel->setChannelName(channelName);

	// total size of a pixel for a single channel (RGBA * float) = 16
	// this is used by Katana to work out how many Channels there are in the image...
	m_pPrimaryChannel->setDataSize(sizeof(float) * 4);

	m_pDataPipe->send(*m_pPrimaryChannel);

	// now do any extra AOVs...

	if (!m_aExtraInteractiveAOVs.empty())
	{
		std::vector<RenderAOV>::iterator itRenderAOV = m_aExtraInteractiveAOVs.begin();
		for (; itRenderAOV != m_aExtraInteractiveAOVs.end(); ++itRenderAOV)
		{
			RenderAOV& rAOV = *itRenderAOV;

#if SEND_ALL_FRAMES
			FnKat::NewFrameMessage* pLocalFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);

			localFrameBufferID = rAOV.frameID;

			// set the name
			std::string frameName;
			FnKat::encodeLegacyName(fFrameName, localFrameBufferID, frameName);
			pLocalFrame->setFrameName(frameName);

			m_pDataPipe->send(*pLocalFrame);
			
			delete pLocalFrame;
#endif

			// this technically speaking leaks, but there's not really that much point freeing it on renderBase::stop(), as the process dies anyway...
			rAOV.pChannelMessage = new FnKat::NewChannelMessage(*m_pFrame, ++channelID, m_renderHeight, m_renderWidth, originX, originY, 1.0f, 1.0f);

			FnKat::encodeLegacyName(rAOV.name, localFrameBufferID, channelName);
			rAOV.pChannelMessage->setChannelName(channelName);

			// total size of a pixel for a single channel (numchannels * float)
			// this is used by Katana to work out how many channels there are in the image...
			rAOV.pChannelMessage->setDataSize(sizeof(float) * rAOV.numDstChannels);

			m_pDataPipe->send(*rAOV.pChannelMessage);
		}
	}
#endif

	return true;
}

void ImagineRender::tileDone(const TileInfo& tileInfo, unsigned int threadID)
{
	if (!m_pOutputImage)
		return;

	const unsigned int origWidth = m_pOutputImage->getWidth();
	const unsigned int origHeight = m_pOutputImage->getHeight();

	unsigned int x = tileInfo.x;
	unsigned int y = tileInfo.y;
	unsigned int width = tileInfo.width;
	unsigned int height = tileInfo.height;

	const bool doAprons = true;
	if (doAprons)
	{
		// add a pixel border around the output tile apron to account for pixel filters...
		const unsigned int tb = tileInfo.tileApronSize;
		if (x >= tb)
		{
			x -= tb;
			width += tb;
		}
		if (y >= tb)
		{
			y -= tb;
			height += tb;
		}
		if (x + width + tb < origWidth)
		{
			width += tb;
		}
		if (y + height + tb < origHeight)
		{
			height += tb;
		}
	}

	// the TileInfo x/y coordinates aren't in local subimage space for crop renders, so we need to offset them
	// back into the OutputImage's space
	unsigned int localSrcX = x;
	unsigned int localSrcY = y;

	if (m_ROIActive)
	{
		// need to clamp these for image apron borders for pixel filtering...
		if (m_ROIStartX > localSrcX)
		{
			localSrcX = 0;
		}
		else
		{
			localSrcX -= m_ROIStartX;
		}

		if (m_ROIStartY > localSrcY)
		{
			localSrcY = 0;
		}
		else
		{
			localSrcY -= m_ROIStartY;
		}
	}

	// take our own copy of a sub-set of the current final output image, containing just our tile area
	OutputImage imageCopy(*m_pOutputImage, localSrcX, localSrcY, width, height);
	if (m_integratorType != 0)
	{
		imageCopy.normaliseProgressive();
	}
	imageCopy.applyExposure(1.1f);

	FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(m_pPrimaryChannel));

	if (!pNewTileMessage)
		return;

	pNewTileMessage->setStartCoordinates(x, y);

	pNewTileMessage->setDataDimensions(width, height);

	unsigned int skipSize = 4 * sizeof(float);
	unsigned int dataSize = width * height * skipSize;
	unsigned char* pData = new unsigned char[dataSize];

	unsigned char* pDstRow = pData;

	//
	x = 0;
	y = 0;

	for (unsigned int i = 0; i < height; i++)
	{
		const Colour4f* pSrcRow = imageCopy.colourRowPtr(y + i);
		const Colour4f* pSrcPixel = pSrcRow + x;

		unsigned char* pDstPixel = pDstRow;

		for (unsigned int j = 0; j < width; j++)
		{
			// copy each component manually, as we need to swap the A channel to be beginning for Katana...
			memcpy(pDstPixel, &pSrcPixel->a, sizeof(float));
			pDstPixel += sizeof(float);
			memcpy(pDstPixel, &pSrcPixel->r, sizeof(float));
			pDstPixel += sizeof(float);
			memcpy(pDstPixel, &pSrcPixel->g, sizeof(float));
			pDstPixel += sizeof(float);
			memcpy(pDstPixel, &pSrcPixel->b, sizeof(float));
			pDstPixel += sizeof(float);

			pSrcPixel += 1;
		}

		pDstRow += width * skipSize;
	}

	pNewTileMessage->setData(pData, dataSize);
	pNewTileMessage->setByteSkip(skipSize);

	m_pDataPipe->send(*pNewTileMessage);

	delete [] pData;
	pData = NULL;

	delete pNewTileMessage;

	// now do any extra AOVs

	std::vector<RenderAOV>::const_iterator itRenderAOV = m_aExtraInteractiveAOVs.begin();
	for (; itRenderAOV != m_aExtraInteractiveAOVs.end(); ++itRenderAOV)
	{
		const RenderAOV& rAOV = *itRenderAOV;

		pNewTileMessage = new FnKat::DataMessage(*(rAOV.pChannelMessage));

		if (!pNewTileMessage)
			continue;

		if (m_ROIActive)
		{
			pNewTileMessage->setStartCoordinates(tileInfo.x, tileInfo.y);
		}
		else
		{
			pNewTileMessage->setStartCoordinates(tileInfo.x, tileInfo.y);
		}
		pNewTileMessage->setDataDimensions(width, height);

		unsigned int skipSize = rAOV.numDstChannels * sizeof(float);
		unsigned int dataSize = width * height * skipSize;
		unsigned char* pData = new unsigned char[dataSize];

		unsigned char* pDstRow = pData;

		//
		x = 0;
		y = 0;
		
		if (rAOV.type == "n")
		{
			for (unsigned int i = 0; i < height; i++)
			{
				const Colour3f* pSrcRow = imageCopy.normalRowPtr(y + i);
				const Colour3f* pSrcPixel = pSrcRow + x;
	
				unsigned char* pDstPixel = pDstRow;
	
				for (unsigned int j = 0; j < width; j++)
				{
					memcpy(pDstPixel, &pSrcPixel->r, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, &pSrcPixel->g, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, &pSrcPixel->b, sizeof(float));
					pDstPixel += sizeof(float);
	
					pSrcPixel += 1;
				}
	
				pDstRow += width * skipSize;
			}
		}
		else if (rAOV.type == "id")
		{
			static float zero = 0.0f;
			
			// if it's ID, we need to copy it into the A channel of the ARGB channel Katana always seems to expect
			for (unsigned int i = 0; i < height; i++)
			{
				const float* pSrcRow = imageCopy.idRowPtr(y + i);
				const float* pSrcPixel = pSrcRow + x;
	
				unsigned char* pDstPixel = pDstRow;
				
				// it's ambiguous what Katana expects here - it *seems* to accept both 3 and 4 channel images,
				// but it's not clear where it want's the single channel items for the ID. The Arnold plugin sets
				// the first channel (A) to 1.0, and then fills in any float ID value from that.
				
				// We seem to be able to successfully send the ID as the first channel and filler values for RGB...
	
				for (unsigned int j = 0; j < width; j++)
				{
					memcpy(pDstPixel, pSrcPixel, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, &zero, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, &zero, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, &zero, sizeof(float));
					pDstPixel += sizeof(float);
		
					pSrcPixel += 1;
				}
	
				pDstRow += width * skipSize;
			}
		}

		pNewTileMessage->setData(pData, dataSize);
		pNewTileMessage->setByteSkip(skipSize);

		m_pDataPipe->send(*pNewTileMessage);

		delete [] pData;
		pData = NULL;

		delete pNewTileMessage;
	}
}

void ImagineRender::sendFullFrameToMonitor()
{
#if ENABLE_PREVIEW_RENDERS
	bool doFinal = true;
	if (doFinal && m_pOutputImage)
	{
		// send through the entire image again...
		OutputImage imageCopy(*m_pOutputImage);

		const unsigned int origWidth = imageCopy.getWidth();
		const unsigned int origHeight = imageCopy.getHeight();

		if (m_integratorType != 0)
		{
			imageCopy.normaliseProgressive();
		}
		imageCopy.applyExposure(1.1f);

		FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(m_pPrimaryChannel));
		pNewTileMessage->setStartCoordinates(m_ROIStartX, m_ROIStartY);
		pNewTileMessage->setDataDimensions(origWidth, origHeight);

		unsigned int skipSize = 4 * sizeof(float);
		unsigned int dataSize = origWidth * origHeight * skipSize;
		unsigned char* pData = new unsigned char[dataSize];

		unsigned char* pDstRow = pData;

		for (unsigned int i = 0; i < origHeight; i++)
		{
			const Colour4f* pSrcRow = imageCopy.colourRowPtr(i);
			const Colour4f* pSrcPixel = pSrcRow;

			unsigned char* pDstPixel = pDstRow;

			for (unsigned int j = 0; j < origWidth; j++)
			{
				// copy each component manually, as we need to swap the A channel to be beginning for Katana...
				memcpy(pDstPixel, &pSrcPixel->a, sizeof(float));
				pDstPixel += sizeof(float);
				memcpy(pDstPixel, &pSrcPixel->r, sizeof(float));
				pDstPixel += sizeof(float);
				memcpy(pDstPixel, &pSrcPixel->g, sizeof(float));
				pDstPixel += sizeof(float);
				memcpy(pDstPixel, &pSrcPixel->b, sizeof(float));
				pDstPixel += sizeof(float);

				pSrcPixel += 1;
			}

			pDstRow += origWidth * skipSize;
		}

		pNewTileMessage->setData(pData, dataSize);
		pNewTileMessage->setByteSkip(skipSize);

		m_pDataPipe->send(*pNewTileMessage);

		delete [] pData;
		pData = NULL;

		delete pNewTileMessage;

		// this blocks until Katana's received everything it was expecting...
//		m_pDataPipe->flushPipe(*m_pPrimaryChannel);
		
		m_pDataPipe->closeChannel(*m_pPrimaryChannel);
	}

	// given that renderboot generally gets killed at this point, we could probably get away with not doing this, but...
	if (m_pOutputImage)
	{
		delete m_pOutputImage;
		m_pOutputImage = NULL;
	}
#endif
}
