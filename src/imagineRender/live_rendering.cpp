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

#include "katana_helpers.h"
#include "material_helper.h"

// Imagine stuff
#include "materials/material.h"
#include "lights/light.h"

#include "utils/logger.h"

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
		
//		fprintf(stderr, "\n\n%s\n\n", dataUpdateItemAttribute.getXML().c_str());

		FnKat::StringAttribute typeAttribute = dataUpdateItemAttribute.getChildByName("type");

		if (!typeAttribute.isValid())
			continue;

		FnKat::StringAttribute locationAttribute = dataUpdateItemAttribute.getChildByName("location");
		FnKat::GroupAttribute attributesAttribute = dataUpdateItemAttribute.getChildByName("attributes");
		
//		fprintf(stderr, "\n\n%s\n\n", attributesAttribute.getXML().c_str());
		
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
			KatanaUpdateItem newUpdate(KatanaUpdateItem::eTypeCamera, KatanaUpdateItem::eLocCamera, location);
			
			FnKat::GroupAttribute xformAttribute = attributesAttribute.getChildByName("xform");
			if (xformAttribute.isValid())
			{
				LiveRenderHelpers::setUpdateXFormFromAttribute(xformAttribute, newUpdate);
			}
			
			FnKat::GroupAttribute geometryAttribute = attributesAttribute.getChildByName("geometry");
			if (geometryAttribute.isValid())
			{
				FnKat::DoubleAttribute fovAttribute = geometryAttribute.getChildByName("fov");
				if (fovAttribute.isValid())
				{
					double fovValue = fovAttribute.getValue(70.0, false);
					newUpdate.extra.add("fov", (float)fovValue);
				}
				
				FnKat::DoubleAttribute nearClipAttribute = geometryAttribute.getChildByName("near");
				if (nearClipAttribute.isValid())
				{
					double nearClipValue = nearClipAttribute.getValue(0.1, false);
					newUpdate.extra.add("nearClip", (float)nearClipValue);
				}
			}
			
			m_liveRenderState.addUpdate(newUpdate);
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
				MaterialHelper materialHelper(m_logger);
				// we have a network material
				
				Material* pNewMaterial = materialHelper.createNetworkMaterial(materialAttribute, false);
				if (pNewMaterial)
				{
					// extremely hacky for the moment...
					KatanaUpdateItem newUpdate(KatanaUpdateItem::eTypeObjectMaterial, KatanaUpdateItem::eLocObject, location);
					
					newUpdate.pMaterial = pNewMaterial;
					
					m_liveRenderState.addUpdate(newUpdate);
				}
			}
		}
		else if (type == "geo")
		{
			KatanaUpdateItem newUpdate(KatanaUpdateItem::eTypeObject, KatanaUpdateItem::eLocObject, location);
			
			bool changed = false;
			
			FnKat::GroupAttribute xformAttribute = attributesAttribute.getChildByName("xform");
			if (xformAttribute.isValid())
			{
				LiveRenderHelpers::setUpdateXFormFromAttribute(xformAttribute, newUpdate);
				changed = true;
			}
			
			FnKat::IntAttribute deletedAttribute = attributesAttribute.getChildByName("deleted");
			if (deletedAttribute.isValid())
			{
				int deletedValue = deletedAttribute.getValue(0, false);
				if (deletedValue == 1)
				{
					newUpdate.extra.add("deleted", true);
					changed = true;
				}
			}
			
			if (changed)
			{
				m_liveRenderState.addUpdate(newUpdate);
			}
		}
		else if (type == "light")
		{
//			fprintf(stderr, "\n\n%s\n\n", attributesAttribute.getXML().c_str());
			
			KatanaUpdateItem newUpdate(KatanaUpdateItem::eTypeLight, KatanaUpdateItem::eLocLight, location);
			
			FnKat::GroupAttribute xformAttribute = attributesAttribute.getChildByName("xform");
			if (xformAttribute.isValid())
			{
				LiveRenderHelpers::setUpdateXFormFromAttribute(xformAttribute, newUpdate);
			}
			
			FnKat::GroupAttribute materialAttribute = attributesAttribute.getChildByName("material");
			if (materialAttribute.isValid())
			{
				FnKat::GroupAttribute shaderParamsAttribute = materialAttribute.getChildByName("imagineLightParams");
				if (shaderParamsAttribute.isValid())
				{
					KatanaAttributeHelper helper(shaderParamsAttribute);
					
					float intensity = helper.getFloatParam("intensity", 1.0f);
					newUpdate.extra.add("intensity", intensity);
					
					float exposure = helper.getFloatParam("exposure", 1.0f);
					newUpdate.extra.add("exposure", exposure);
				}
			}
			
			FnKat::IntAttribute muteAttribute = attributesAttribute.getChildByName("mute");
			if (muteAttribute.isValid())
			{
				int muteValue = muteAttribute.getValue(0, false);
				newUpdate.extra.add("muted", (bool)muteValue);
			}
			
			m_liveRenderState.addUpdate(newUpdate);
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
	m_logger.debug("applyPendingDataUpdates() called");
	
	m_liveRenderState.lock();
	
	// stop tracing as early as possible, so the render threads can shut down
	m_pRaytracer->terminate();
	
	std::vector<KatanaUpdateItem>::const_iterator itUpdate = m_liveRenderState.updatesBegin();
	for (; itUpdate != m_liveRenderState.updatesEnd(); ++itUpdate)
	{
		const KatanaUpdateItem& update = *itUpdate;

		if (update.type == KatanaUpdateItem::eTypeCamera)
		{
			// get hold of camera
			Camera* pCamera = m_pScene->getRenderCamera();

			if (update.haveXForm)
			{
				pCamera->transform().setCachedMatrix(update.xform.data(), true);
			}
			
			m_logger.debug("Updating Camera matrix");
			
			if (!update.extra.isEmpty())
			{
				pCamera->setFOV(update.extra.getFloat("fov", 70.0f));
				pCamera->setNearClippingPlane(update.extra.getFloat("nearClip", 0.1f));
			}
		}
		else if (update.type == KatanaUpdateItem::eTypeObjectMaterial && update.pMaterial)
		{
			Object* pLocationObject = m_pScene->getObjectByName(update.location);
			
			if (!pLocationObject)
			{
				m_logger.error("Can't find object: %s in scene in order to update its material.", update.location.c_str());
				continue;
			}
			
			update.pMaterial->preRenderMaterial();
			pLocationObject->setMaterial(update.pMaterial);
		}
		else if (update.type == KatanaUpdateItem::eTypeObject)
		{
			Object* pLocationObject = m_pScene->getObjectByName(update.location);
			
			if (!pLocationObject)
			{
				m_logger.error("Can't find object: %s in scene in order to update its properties.", update.location.c_str());
				continue;
			}
			
			m_logger.debug("Updating properties of object: %s", update.location.c_str());
			
			if (pLocationObject->getRenderVisibilityFlags() == 0)
			{
				// we can cheat here, and assume that if we've made it invisible before, we need to add it again.
				// This isn't really correct, as we can't tell the difference between deleted and hidden, but...
				pLocationObject->setRenderVisibilityFlags(RENDER_VISIBILITY_FLAGS_ALL);
			}
			else
			{
				if (update.extra.getBool("deleted", false))
				{
					// hacky and wrong, but...
					pLocationObject->setRenderVisibilityFlags(0);
				}
			}			
			
			if (update.haveXForm)
			{
				pLocationObject->transform().setCachedMatrix(update.xform.data(), true);
			}
		}
		else if (update.type == KatanaUpdateItem::eTypeLight)
		{
			Light* pLocationLight = m_pScene->getLightByName(update.location);
			
			if (!pLocationLight)
			{
				m_logger.error("Can't find light: %s in scene in order to update its parameters.", update.location.c_str());
				continue;
			}
			
			m_logger.debug("Updating properties of light: %s", update.location.c_str());
			
			if (update.haveXForm)
			{
				pLocationLight->transform().setCachedMatrix(update.xform.data(), true);
			}
			
			pLocationLight->setMuted(update.extra.getBool("muted", false));
			
			pLocationLight->setIntensity(update.extra.getFloat("intensity", 1.0f));
			pLocationLight->setExposure(update.extra.getFloat("exposure", 1.0f));
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
	
	m_logger.debug("Restarting render");
	
	m_pRaytracer->renderScene(1.0f, &m_renderSettings, false);
}
