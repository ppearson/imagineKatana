#include "imagine_render.h"

#include "katana_helpers.h"

using namespace Imagine;

// splitting the functions from the same class into separate files is a nasty thing to do, but 
// there's so much shared state that for the moment, it's a lot easier...

int ImagineRender::queueDataUpdates(FnKat::GroupAttribute updateAttribute)
{
//	fprintf(stderr, "Queue updates...\n");
	
	m_liveRenderState.lock();
	if (!m_pRaytracer)
	{
		m_liveRenderState.unlock();
		return 0;
	}

	m_liveRenderState.unlock();

	unsigned int numItems = updateAttribute.getNumberOfChildren();

	for (unsigned int i = 0; i < numItems; i++)
	{
		FnKat::GroupAttribute dataUpdateItemAttribute = updateAttribute.getChildByIndex(i);

		if (!dataUpdateItemAttribute.isValid())
			continue;

		FnKat::StringAttribute typeAttribute = dataUpdateItemAttribute.getChildByName("type");

		if (!typeAttribute.isValid())
			continue;

		FnKat::StringAttribute locationAttribute = dataUpdateItemAttribute.getChildByName("location");
		FnKat::GroupAttribute attributesAttribute = dataUpdateItemAttribute.getChildByName("attributes");
		
//		fprintf(stderr, "\n\n\n%s\n\n\n", attributesAttribute.getXML().c_str());

		bool partialUpdate = false;

		FnKat::StringAttribute partialUpdateAttribute = attributesAttribute.getChildByName("partialUpdate");
		if (partialUpdateAttribute.isValid() && partialUpdateAttribute.getValue("", false) == "True")
		{
			partialUpdate = true;
		}

		std::string type = typeAttribute.getValue("", false);
		std::string location = locationAttribute.getValue("", false);

		if (location == m_renderCameraLocation)
		{
			FnKat::GroupAttribute xformAttribute = attributesAttribute.getChildByName("xform");

			if (xformAttribute.isValid())
			{
				KatanaUpdateItem newUpdate;
				newUpdate.camera = true;

				newUpdate.xform.resize(16);

				FnKat::DoubleAttribute xformMatrixAttribute = xformAttribute.getChildByName("global");			
				if (xformMatrixAttribute.isValid())
				{
					FnKat::DoubleConstVector matrixValues = xformMatrixAttribute.getNearestSample(0.0f);
	
					if (matrixValues.size() != 16)
						continue;					
					
					std::copy(matrixValues.begin(), matrixValues.end(), newUpdate.xform.begin());
				}
				else
				{
					// see if there's an interactive one
					
					FnKat::GroupAttribute xformInteractiveAttribute = xformAttribute.getChildByName("interactive");
					if (xformInteractiveAttribute.isValid())
					{
						FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(xformInteractiveAttribute);
						const double* pMatrix = xforms[0].getValues();
						for (unsigned int i = 0; i < 16; i++)
						{
							newUpdate.xform[i] = pMatrix[i];
						}
					}
				}				

				m_liveRenderState.addUpdate(newUpdate);
			}
		}
	}

	return 0;
}

bool ImagineRender::hasPendingDataUpdates() const
{
//	fprintf(stderr, "hasPendingDataUpdates()\n");
	return m_liveRenderState.hasUpdates();
}

int ImagineRender::applyPendingDataUpdates()
{
//	fprintf(stderr, "applyPendingDataUpdates()\n");
	
	m_liveRenderState.lock();
	
	m_pRaytracer->terminate();
	
	::usleep(500);

	std::vector<KatanaUpdateItem>::const_iterator itUpdate = m_liveRenderState.updatesBegin();
	for (; itUpdate != m_liveRenderState.updatesEnd(); ++itUpdate)
	{
		const KatanaUpdateItem& update = *itUpdate;

		if (update.camera)
		{
			// get hold of camera
			Camera* pCamera = m_pScene->getRenderCamera();

			pCamera->transform().setCachedMatrix(update.xform.data(), true);
			
//			fprintf(stderr, "Updating Camera\n");
		}
	}

	m_liveRenderState.unlock();

	m_liveRenderState.flushUpdates();

	restartLiveRender();

	return 0;
}

void ImagineRender::restartLiveRender()
{
	m_liveRenderState.lock();

//	::usleep(500);

	m_pOutputImage->clearImage();
/*
	if (m_pRaytracer)
	{
		delete m_pRaytracer;
		m_pRaytracer = NULL;
	}

	m_pRaytracer = new Raytracer(*m_pScene, m_renderThreads, true);
	m_pRaytracer->setExtraChannels(0);
	m_pRaytracer->setHost(this);

	m_pRaytracer->initialise(m_pOutputImage, m_renderSettings);
*/
	m_liveRenderState.unlock();
	
//	fprintf(stderr, "Restarting render\n");

	m_pRaytracer->renderScene(1.0f, &m_renderSettings, false);
}
