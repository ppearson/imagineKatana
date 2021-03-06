/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

#include "imagine_render.h"

using namespace Imagine;

// send unique frames for each channel - older example render plugins used to do this explicitly,
// the newer plugins effectively do this via odd frameID map they use (which doesn't seem to actually
// ever cache anything in terms of setting up the NewFrame/NewChannel messages...)
#define SEND_ALL_FRAMES 1

// doing this seems to require the channel names to actually be the encoded frame name, otherwise Katana randomly
// interleaves the channels, leaving scrambled buffers with more than just the primary channel
#define INCREMENTAL_CHANNEL_IDS 0

// the example plugins and RfK/KtoA do this, but I don't know why - it doesn't seem to change anything
#define USE_PIPE_PER_CHANNEL 0

// splitting the functions from the same class into separate files is a nasty thing to do, but 
// there's so much shared state that for the moment, it's a lot easier, and with all these #ifdefs
// to try and work out what's actually needed, it makes things cleaner for the moment...
// TODO: This stuff in here can be encapsulated in a class quite easily...

#if KATANA_V3
#define USE_KAT3_ZERO_COPY_DATA 1
#define USE_KAT3_RGBA_ORDER 0 // using this currently breaks id buffer when registered as 1 float channel...
#else
#define USE_KAT3_ZERO_COPY_DATA 0
#define USE_KAT3_RGBA_ORDER 0
#endif

#define USE_1CHANNEL_ID_BUFFER 1

bool ImagineRender::setupPreviewDataChannel(Foundry::Katana::Render::RenderSettings& settings)
{
	m_interactiveFrameID = -1;
	m_rawKatanaHost = getKatanaHost();

	size_t portSep = m_rawKatanaHost.find(':');

	if (portSep == std::string::npos)
	{
		m_logger.error("Katana host and port not specified...");
		return false;
	}

	m_katanaHost = m_rawKatanaHost.substr(0, portSep);

	std::string strPort = m_rawKatanaHost.substr(portSep + 1);
	m_katanaPort = atol(strPort.c_str());

	// Katana requires the port number retrieved above to be incremented by 100,
	// despite the fact that KatanaPipe::connect() seems to handle doing that itself for the control
	// port (which is +100 the data port). Not doing this causes Katana to segfault on the connect()
	// call, leaving renderboot orphaned doing the render in the background...
	m_katanaPort += 100;

	// customise channels we're going to do from settings...
	bool doExtaChannels = true;
	
#if INCREMENTAL_CHANNEL_IDS
	int channelID = 0;
#endif

	FnKat::Render::RenderSettings::ChannelBuffers interactiveRenderBuffers;
	settings.getChannelBuffers(interactiveRenderBuffers);

	FnKat::Render::RenderSettings::ChannelBuffers::const_iterator itBuffer = interactiveRenderBuffers.begin();
	for (; itBuffer != interactiveRenderBuffers.end(); ++itBuffer)
	{
		const FnKat::Render::RenderSettings::ChannelBuffer& buffer = (*itBuffer).second;

		const std::string& channelBufferName = (*itBuffer).first;
			
		std::string channelType = buffer.channelName;
		
		int channelBufferID = atoi(buffer.bufferId.c_str());
		
#if INCREMENTAL_CHANNEL_IDS
		int localChannelID = channelID++;
#else
		int localChannelID = channelBufferID;
#endif	
		
		// detect the primary main channel
		if (channelBufferName == "0primary" && channelType == "rgba")
		{
//			channelBufferName == "primary";
			m_interactivePrimaryChannelName = channelBufferName;
			
			// frame ID of render automatically becomes the primary output's channel buffer ID
			m_interactiveFrameID = channelBufferID;
			
			// in this case, for the primary channel, interactive frame ID and channel ID are the same
			m_aInteractiveChannels.push_back(RenderChannel(channelBufferName, channelType, 4, 4, m_interactiveFrameID, localChannelID));
		}

		// detect extra AOVs...

		if (doExtaChannels && channelType == "n")
		{
			// Katana does seem to support 3 channel "channels", and sets a 1.0 alpha for all the image in this case.
			m_aInteractiveChannels.push_back(RenderChannel(channelBufferName, channelType, 3, 3, m_interactiveFrameID, localChannelID));
			m_renderSettings.add("output_normals", true);
			m_extraAOVsFlags |= COMPONENT_NORMAL;
		}
		else if (doExtaChannels && channelType == "wpp")
		{
			m_renderSettings.add("output_wpp", true);
			m_extraAOVsFlags |= COMPONENT_WPP;
		}

		m_logger.debug("Interactive Output Buffer: %s: %i, %s", channelBufferName.c_str(), localChannelID, channelType.c_str());
	}
	
	if (m_enableIDPicking)
	{
		// Katana doesn't include ID channel in results from getChannelBuffers() so we have to just add it...
		
		std::string channelName = "__id";
		std::string channelType = "id";
		
		// IDs have to be the same here...
		
		// Despite Katana accepting 3-channel channels in the N case (above), and automatically filling in a constant 1.0 alpha for it,
		// the ID channel seems to be handled separately, and can only be a 1- or 4-channel channel.
#if USE_1CHANNEL_ID_BUFFER
		// if it's a single channel, Katana will accept it as if the destination literally is a single float channel * image dimensions,
		// as long as Katana 3.0's new RGBA order isn't enabled - if it is, then it doesn't work, indicating Katana is still backing this
		// channel internally as a 4-channel buffer?!
		m_aInteractiveChannels.push_back(RenderChannel(channelName, channelType, 1, 1, m_interactiveFrameID, m_interactiveFrameID));
#else
		
		// Can't seem to get IDs to work at all with 3-channel buffers - putting the ID in R, G or B doesn't work.
		// With 4-channel buffers it works.
		m_aInteractiveChannels.push_back(RenderChannel(channelName, channelType, 1, 4, m_interactiveFrameID, m_interactiveFrameID));
#endif
		m_extraAOVsFlags |= COMPONENT_ID;

		m_renderSettings.add("output_ids", true);		
	}

	if (m_aInteractiveChannels.empty())
	{
		m_logger.error("No channel buffers specified for interactive rendering...");
		return false;
	}

	//

	
#if USE_PIPE_PER_CHANNEL
#else	
	if (m_pDataPipe)
	{
		delete m_pDataPipe;
		m_pDataPipe = NULL;
	}

	m_pDataPipe = FnKat::PipeSingleton::Instance(m_katanaHost, m_katanaPort);

	// try and connect...
	if (m_pDataPipe->connect() != 0)
	{
		m_logger.error("Can't connect to Katana data socket: %s:%u", m_katanaHost.c_str(), m_katanaPort);
		return false;
	}
#endif
	
	unsigned int originX = 0;
	unsigned int originY = 0;
/*
	if (m_ROIActive)
	{
		originX += m_ROIStartX;
		originY += m_ROIStartY;
	}
*/

#if USE_PIPE_PER_CHANNEL
#else
	if (m_pFrame)
	{
		delete m_pFrame;
		m_pFrame = NULL;
	}

	// technically leaks, but...
	m_pFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);

	// set the name
	std::string frameName;
	FnKat::encodeLegacyName(m_interactivePrimaryChannelName, m_interactiveFrameID, frameName);
	m_pFrame->setFrameName(frameName);
	
	m_pFrame->setFrameTime(getRenderTime());
#endif

#if USE_PIPE_PER_CHANNEL
#else
	m_pDataPipe->send(*m_pFrame);
#endif

	unsigned int channelIndex = 0;
	std::vector<RenderChannel>::iterator itRenderChannel = m_aInteractiveChannels.begin();
	for (; itRenderChannel != m_aInteractiveChannels.end(); ++itRenderChannel)
	{
		RenderChannel& rChannel = *itRenderChannel;
		
		FnKat::NewFrameMessage* pMainFrame = NULL;

#if SEND_ALL_FRAMES
		FnKat::NewFrameMessage* pLocalFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);

//		int localFrameBufferID = rChannel.frameID;
		int localFrameBufferID = m_interactiveFrameID;

		// set the name
		std::string frameName;
		FnKat::encodeLegacyName(m_interactivePrimaryChannelName, localFrameBufferID, frameName);
		pLocalFrame->setFrameName(frameName);

		m_pDataPipe->send(*pLocalFrame);
		
		pMainFrame = pLocalFrame;
#else
		pMainFrame = m_pFrame;
#endif
		
		int localChannelID = rChannel.channelID;
		
#if USE_PIPE_PER_CHANNEL
		// create a new data pipe for the channel, and send everything down it
		rChannel.pDataPipe = FnKat::PipeSingleton::Instance(m_katanaHost, m_katanaPort);
		
		// try and connect...
		if (rChannel.pDataPipe->connect() != 0)
		{
			m_logger.error("Can't connect to Katana channel data socket: %s:%u", m_katanaHost.c_str(), m_katanaPort);
			return false;
		}
		
		// if we're the first channel (so the first data pipe), create the frame and send the frame message down the pipe
		if (channelIndex == 0)
		{
			// technically leaks, but...
			m_pFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);
		
			// set the name
			std::string frameName;
			FnKat::encodeLegacyName(m_interactivePrimaryChannelName, m_interactiveFrameID, frameName);
			m_pFrame->setFrameName(frameName);
			
			m_pFrame->setFrameTime(getRenderTime());
			
			rChannel.pDataPipe->send(*m_pFrame);
		}
		
		pMainFrame = m_pFrame;
#endif
		
#if USE_KAT3_RGBA_ORDER
		// this technically speaking leaks, but there's not really that much point freeing it on renderBase::stop(), as the process dies anyway...
		rChannel.pChannelMessage = new FnKat::NewChannelMessage_v2(*pMainFrame, localChannelID, FnKat::NewChannelMessage_v2::RGBA,
																   m_renderHeight, m_renderWidth, originX, originY, 1.0f, 1.0f);
#else
		// this technically speaking leaks, but there's not really that much point freeing it on renderBase::stop(), as the process dies anyway...
		rChannel.pChannelMessage = new FnKat::NewChannelMessage(*pMainFrame, localChannelID, m_renderHeight, m_renderWidth, originX, originY, 1.0f, 1.0f);
#endif

		std::string channelName;
#if INCREMENTAL_CHANNEL_IDS
		// It's confusing what to do here - the docs say one thing, the example render plugins do another...
		// It appears that if you use locally incrementing channel IDs (as the docs suggest), the channel name
		// actually has to be identical to the frame name, which is at odds with what the docs say
		FnKat::encodeLegacyName(m_interactivePrimaryChannelName, m_interactiveFrameID, channelName);
#else
		FnKat::encodeLegacyName(rChannel.name, localChannelID, channelName);
#endif
		rChannel.pChannelMessage->setChannelName(channelName);

		// total size of a pixel for a single channel (numchannels * float)
		// this is used by Katana to work out how many channels there are in the image...
		rChannel.pChannelMessage->setDataSize(sizeof(float) * rChannel.numDstChannels);
		
#if USE_PIPE_PER_CHANNEL
		rChannel.pDataPipe->send(*rChannel.pChannelMessage);
#else
		m_pDataPipe->send(*rChannel.pChannelMessage);
#endif
		
#if SEND_ALL_FRAMES
		delete pLocalFrame;
#endif
		
		channelIndex++;		
	}

	return true;
}

#if USE_KAT3_ZERO_COPY_DATA
void ImageDataBufferDeleter(void* pData)
{
    char* dataArray = reinterpret_cast<char*>(pData);
    delete [] dataArray;
}
#endif

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

	const bool doAprons = false;
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
	imageCopy.applyExposure(1.0f);

	std::vector<RenderChannel>::const_iterator itRenderChannel = m_aInteractiveChannels.begin();
	for (; itRenderChannel != m_aInteractiveChannels.end(); ++itRenderChannel)
	{
		const RenderChannel& rChannel = *itRenderChannel;

		FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(rChannel.pChannelMessage));

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

		unsigned int skipSize = rChannel.numDstChannels * sizeof(float);
		unsigned int dataSize = width * height * skipSize;
		unsigned char* pData = new unsigned char[dataSize];

		unsigned char* pDstRow = pData;

		//
		x = 0;
		y = 0;
		
		if (rChannel.type == "rgba")
		{
			for (unsigned int i = 0; i < height; i++)
			{
				const Colour4f* pSrcRow = imageCopy.colourRowPtr(y + i);
				const Colour4f* pSrcPixel = pSrcRow + x;
				
#if USE_KAT3_RGBA_ORDER
				memcpy(pDstRow, &pSrcPixel->r, width * skipSize);
#else
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
#endif
		
				pDstRow += width * skipSize;
			}
		}
		else if (rChannel.type == "n")
		{		
			for (unsigned int i = 0; i < height; i++)
			{
				const Colour3f* pSrcRow = imageCopy.normalRowPtr(y + i);
				const Colour3f* pSrcPixel = pSrcRow + x;
	
				memcpy(pDstRow, &pSrcPixel->r, width * skipSize);
	
				pDstRow += width * skipSize;
			}
		}
		else if (rChannel.type == "id")
		{
			// if it's ID, we need to copy it into the A channel of the ARGB channel Katana always seems to expect
			for (unsigned int i = 0; i < height; i++)
			{
				const float* pSrcRow = imageCopy.idRowPtr(y + i);
				const float* pSrcPixel = pSrcRow + x;
				
				// it's ambiguous what Katana expects here - it *seems* to accept 1, 3 and 4 channel images,
				// but it's not clear where it wants the single channel items for the ID in the latter two cases. The Arnold plugin sets
				// the first channel (A) to 1.0, and then fills in any float ID value from that.
				
				// We seem to be able to successfully send the ID as the first channel and filler values for RGB in these cases...
				
#if USE_1CHANNEL_ID_BUFFER
				// Sending a single channel with just the ID also seems to work...
				// As long as we don't use the new RGBA order in Katana 3, in which case it doesn't.
				memcpy(pDstRow, pSrcPixel, width * skipSize);
#else
				unsigned char* pDstPixel = pDstRow;
			#if USE_KAT3_RGBA_ORDER
				pDstPixel += sizeof(float) * 3;
			#endif
				
				for (unsigned int j = 0; j < width; j++)
				{
					memcpy(pDstPixel, pSrcPixel, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, pSrcPixel, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, pSrcPixel, sizeof(float));
					pDstPixel += sizeof(float);
					memcpy(pDstPixel, pSrcPixel, sizeof(float));
					pDstPixel += sizeof(float);
		
					pSrcPixel += 1;
				}
#endif
				pDstRow += width * skipSize;
			}
		}

#if USE_KAT3_ZERO_COPY_DATA
		pNewTileMessage->setData(pData, dataSize, ImageDataBufferDeleter);
#else
		pNewTileMessage->setData(pData, dataSize);
#endif
		pNewTileMessage->setByteSkip(skipSize);

#if USE_PIPE_PER_CHANNEL
		rChannel.pDataPipe->send(*pNewTileMessage);
#else
		m_pDataPipe->send(*pNewTileMessage);
#endif

#if USE_KAT3_ZERO_COPY_DATA
#else
		delete [] pData;
		pData = NULL;
#endif

		delete pNewTileMessage;
		pNewTileMessage = NULL;
	}
}

void ImagineRender::sendFullFrameToMonitor()
{
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
		imageCopy.applyExposure(1.0f);
		
		std::vector<RenderChannel>::const_iterator itRenderChannel = m_aInteractiveChannels.begin();
		for (; itRenderChannel != m_aInteractiveChannels.end(); ++itRenderChannel)
		{
			const RenderChannel& rChannel = *itRenderChannel;
	
			FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(rChannel.pChannelMessage));
	
			if (!pNewTileMessage)
				continue;
	
			pNewTileMessage->setStartCoordinates(m_ROIStartX, m_ROIStartY);
			pNewTileMessage->setDataDimensions(origWidth, origHeight);
	
			unsigned int skipSize = rChannel.numDstChannels * sizeof(float);
			unsigned int dataSize = origWidth * origHeight * skipSize;
			unsigned char* pData = new unsigned char[dataSize];
	
			unsigned char* pDstRow = pData;
			
			if (rChannel.type == "rgba")
			{
				for (unsigned int i = 0; i < origHeight; i++)
				{
					const Colour4f* pSrcRow = imageCopy.colourRowPtr(i);
					const Colour4f* pSrcPixel = pSrcRow;
			
					unsigned char* pDstPixel = pDstRow;
#if USE_KAT3_RGBA_ORDER
					memcpy(pDstRow, &pSrcPixel->r, origWidth * skipSize);
#else			
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
#endif
			
					pDstRow += origWidth * skipSize;
				}
			}
			else if (rChannel.type == "n")
			{				
				for (unsigned int i = 0; i < origHeight; i++)
				{
					const Colour3f* pSrcRow = imageCopy.normalRowPtr(i);
					const Colour3f* pSrcPixel = pSrcRow;
		
					memcpy(pDstRow, &pSrcPixel->r, origWidth * skipSize);
		
					pDstRow += origWidth * skipSize;
				}
			}
			else if (rChannel.type == "id")
			{
				// if it's ID, we need to copy it into the A channel of the ARGB channel Katana always seems to expect
				for (unsigned int i = 0; i < origHeight; i++)
				{
					const float* pSrcRow = imageCopy.idRowPtr(i);
					const float* pSrcPixel = pSrcRow;
					
					// it's ambiguous what Katana expects here - it *seems* to accept 1, 3 and 4 channel images,
					// but it's not clear where it wants the single channel items for the ID in the latter two cases. The Arnold plugin sets
					// the first channel (A) to 1.0, and then fills in any float ID value from that.
					
					// We seem to be able to successfully send the ID as the first channel and filler values for RGB in these cases for 4-channel buffers,
					// which produce the correct result, as long as Katana 3's new RGBA order isn't used.
		
#if USE_1CHANNEL_ID_BUFFER
					// Sending a single channel with just the ID also seems to work...
					// As long as we don't use the new RGBA order in Katana 3, in which case it doesn't.
					memcpy(pDstRow, pSrcPixel, origWidth * skipSize);
#else
					unsigned char* pDstPixel = pDstRow;
				#if USE_KAT3_RGBA_ORDER
//					pDstPixel += sizeof(float) * 3;
				#endif
					
					// setting all RGBA pixels to the ID value in the case of using Katana 3's RGBA order doesn't work either...
					// Saving out the catalog's image for the ID buffer shows the correct ID value is in the R channel (all other values are 0.0) for
					// the case of ARGB order (default, and pre-Katana 3), but with RGBA order registered for the chanel, the ID value is in the
					// A channel (all other values are 0.0), despite all RGBA input values being set to the ID, and picking doesn't work correctly.
					for (unsigned int j = 0; j < origWidth; j++)
					{
						memcpy(pDstPixel, pSrcPixel, sizeof(float));
						pDstPixel += sizeof(float);
						memcpy(pDstPixel, pSrcPixel, sizeof(float));
						pDstPixel += sizeof(float);
						memcpy(pDstPixel, pSrcPixel, sizeof(float));
						pDstPixel += sizeof(float);
						memcpy(pDstPixel, pSrcPixel, sizeof(float));
						pDstPixel += sizeof(float);
			
						pSrcPixel += 1;
					}	
#endif
					pDstRow += origWidth * skipSize;
				}
			}
			
#if USE_KAT3_ZERO_COPY_DATA
			pNewTileMessage->setData(pData, dataSize, ImageDataBufferDeleter);
#else
			pNewTileMessage->setData(pData, dataSize);
#endif
			pNewTileMessage->setByteSkip(skipSize);
	
#if USE_PIPE_PER_CHANNEL
			rChannel.pDataPipe->send(*pNewTileMessage);
#else
			m_pDataPipe->send(*pNewTileMessage);
#endif

#if USE_KAT3_ZERO_COPY_DATA
#else
			delete [] pData;
			pData = NULL;
#endif	
			delete pNewTileMessage;
			pNewTileMessage = NULL;
			
#if USE_PIPE_PER_CHANNEL
			rChannel.pDataPipe->flushPipe(*rChannel.pChannelMessage);
#else
			// this blocks until Katana's received everything it was expecting...
			m_pDataPipe->flushPipe(*rChannel.pChannelMessage);
#endif
			
#if USE_PIPE_PER_CHANNEL
			rChannel.pDataPipe->closeChannel(*rChannel.pChannelMessage);
#else
			m_pDataPipe->closeChannel(*rChannel.pChannelMessage);
#endif
		}
		
		if (m_pFrame)
		{
			delete m_pFrame;
			m_pFrame = NULL;
		}
	}
	
	// given that renderboot generally gets killed at this point, we could probably get away with not doing this, but...
	if (m_pOutputImage)
	{
		delete m_pOutputImage;
		m_pOutputImage = NULL;
	}
}
