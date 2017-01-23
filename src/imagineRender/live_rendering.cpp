#include "imagine_render.h"

#include "katana_helpers.h"
#include "material_helper.h"

#include "materials/material.h"

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
		
//		fprintf(stderr, "\n\n\n%s\n\n\n", dataUpdateItemAttribute.getXML().c_str());

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

		if (type == "camera" && location == m_renderCameraLocation)
		{
			FnKat::GroupAttribute xformAttribute = attributesAttribute.getChildByName("xform");

			if (xformAttribute.isValid())
			{
				KatanaUpdateItem newUpdate(KatanaUpdateItem::eTypeCamera, KatanaUpdateItem::eLocCamera, location);

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
		else if (type == "geoMaterial")
		{
			FnKat::GroupAttribute materialAttribute = attributesAttribute.getChildByName("material");
			
			if (!materialAttribute.isValid())
				continue;
			
			FnKat::StringAttribute shaderAttribute = materialAttribute.getChildByName("imagineSurfaceShader");
			if (shaderAttribute.isValid())
			{
				// we have a none-network material
				
				FnKat::GroupAttribute shaderParams = materialAttribute.getChildByName("imagineSurfaceParams");
				// if all shader params are default, there won't be a group, so shaderParams will be invalid, but we can pass
				// this down anyway. So we don't need to check for the validity of shaderParams, as its possible non-existance
				// is okay.
				
				// extremely hacky for the moment...
				KatanaUpdateItem newUpdate(KatanaUpdateItem::eTypeObjectMaterial, KatanaUpdateItem::eLocObject, location);
				
				std::string shaderType = shaderAttribute.getValue("", false);
				
				Material* pNewMaterial = MaterialHelper::createNewMaterialStandAlone(shaderType, shaderParams);
				
				if (pNewMaterial)
				{
					newUpdate.pMaterial = pNewMaterial;
					
					m_liveRenderState.addUpdate(newUpdate);
				}
				
				continue;
			}
			
			FnKat::GroupAttribute nodesAttribute = materialAttribute.getChildByName("nodes");
			if (nodesAttribute.isValid())
			{
				
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
	
	// stop tracing as early as possible, so the render threads can shut down
	m_pRaytracer->terminate();
	
	// for the moment, we need to pause a bit for some update types, but this should really be behind an interface in Imagine...
	::usleep(500);

	std::vector<KatanaUpdateItem>::const_iterator itUpdate = m_liveRenderState.updatesBegin();
	for (; itUpdate != m_liveRenderState.updatesEnd(); ++itUpdate)
	{
		const KatanaUpdateItem& update = *itUpdate;

		if (update.type == KatanaUpdateItem::eTypeCamera)
		{
			// get hold of camera
			Camera* pCamera = m_pScene->getRenderCamera();

			pCamera->transform().setCachedMatrix(update.xform.data(), true);
			
//			fprintf(stderr, "Updating Camera\n");
		}
		else if (update.type == KatanaUpdateItem::eTypeObjectMaterial && update.pMaterial)
		{
			Object* pLocationObject = m_pScene->getObjectByName(update.location);
			
			if (!pLocationObject)
			{
				fprintf(stderr, "Can't find object: %s in scene to update material of.\n", update.location.c_str());
				continue;
			}
			
			update.pMaterial->preRenderMaterial();
			pLocationObject->setMaterial(update.pMaterial);
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

	m_pOutputImage->clearImage();

	m_liveRenderState.unlock();
	
//	fprintf(stderr, "Restarting render\n");

	m_pRaytracer->renderScene(1.0f, &m_renderSettings, false);
}