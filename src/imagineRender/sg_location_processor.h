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

#ifndef SG_LOCATION_PROCESSOR_H
#define SG_LOCATION_PROCESSOR_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#include <map>
#include <vector>

#include "material_helper.h"
#include "light_helpers.h"
#include "misc_helpers.h"

#include "materials/material.h"
#include "scene.h"

namespace Imagine
{
	class StandardGeometryInstance;
	class CompactGeometryInstance;
	class GeometryInstance;
	class CompoundObject;
	class Object;
	class Logger;
}

class IDState;

class SGLocationProcessor
{
public:
	SGLocationProcessor(Imagine::Scene& scene, Imagine::Logger& logger, const CreationSettings& creationSettings, IDState* pIDState);
	~SGLocationProcessor();

	struct InstanceInfo
	{
		InstanceInfo() : m_compound(false), pGeoInstance(NULL), haveXForm(false), pSingleItemMaterial(NULL)
		{
			
		}

		bool		m_compound;

		union
		{
			Imagine::GeometryInstance*			pGeoInstance;
			Imagine::CompoundObject*			pCompoundObject;
		};
		
		bool									haveXForm;
		Imagine::Matrix4						xform;
		
		// TODO: could used tagged pointer here for the compound flag...
		Imagine::Material*						pSingleItemMaterial;
	};

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void getFinalMaterials(std::vector<Imagine::Material*>& aMaterials);
	
	void setIsLiveRender(bool liveRender) { m_isLiveRender = liveRender; }

protected:
	
	void addObjectToScene(Imagine::Object* pObject, const FnKat::FnScenegraphIterator& sgIterator);
	
	void registerGeometryInstance(Imagine::GeometryInstance* pGeoInstance);

	void processLocationRecursive(const FnKat::FnScenegraphIterator& iterator, unsigned int currentDepth);

	void processGeometryPolymeshCompact(const FnKat::FnScenegraphIterator& iterator, bool asSubD);

	void processSpecialisedType(const FnKat::FnScenegraphIterator& iterator, unsigned int currentDepth);

	Imagine::CompactGeometryInstance* createCompactGeometryInstanceFromLocation(const FnKat::FnScenegraphIterator& iterator, bool asSubD,
																	   const FnKat::GroupAttribute& imagineStatements);
	Imagine::CompactGeometryInstance* createCompactGeometryInstanceFromLocationDiscard(const FnKat::FnScenegraphIterator& iterator, bool asSubD,
																	   const FnKat::GroupAttribute& imagineStatements);

	Imagine::CompoundObject* createCompoundObjectFromLocation(const FnKat::FnScenegraphIterator& iterator, unsigned int baseLevelDepth);

	void createCompoundObjectFromLocationRecursive(const FnKat::FnScenegraphIterator& iterator, std::vector<Imagine::Object*>& aObjects,
												   unsigned int baseLevelDepth, unsigned int currentDepth);

	InstanceInfo findOrBuildInstanceSourceItem(const FnKat::FnScenegraphIterator& iterator, const std::string& instanceSourcePath);
	void processInstance(const FnKat::FnScenegraphIterator& iterator);
	void processInstanceArray(const FnKat::FnScenegraphIterator& iterator);
	
	void processSphere(const FnKat::FnScenegraphIterator& iterator);

	void processLight(const FnKat::FnScenegraphIterator& iterator);

	static unsigned char getRenderVisibilityFlags(const FnKat::GroupAttribute& imagineStatements);
	static void processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Imagine::Object* pObject);

	unsigned int processUVs(FnKat::FloatConstVector& uvlist, std::vector<Imagine::UV>& aUVs);
	
	unsigned int sendObjectID(const FnKat::FnScenegraphIterator& iterator);
	
	unsigned int getCustomGeoFlags();
	
	Imagine::Logger& getLogger()
	{
		return m_logger;
	}

protected:
	Imagine::Scene&				m_scene;
	Imagine::Logger&			m_logger;
	
	const CreationSettings&		m_creationSettings;

	MaterialHelper				m_materialHelper;
	LightHelpers				m_lightHelper;

	std::map<std::string, InstanceInfo>	m_aInstances;
	
	IDState*					m_pIDState; // we don't own this, and it's optional
	
	bool						m_isLiveRender;
};

#endif // SG_LOCATION_PROCESSOR_H
