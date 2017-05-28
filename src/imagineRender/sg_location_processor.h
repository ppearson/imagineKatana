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
		InstanceInfo() : m_compound(false), pGeoInstance(NULL), pSingleItemMaterial(NULL)
		{
			
		}

		bool		m_compound;

		union
		{
			Imagine::GeometryInstance*			pGeoInstance;
			Imagine::CompoundObject*			pCompoundObject;
		};
		
		// TODO: could used tagged pointer here for the compound flag...
		Imagine::Material*						pSingleItemMaterial;
	};

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void getFinalMaterials(std::vector<Imagine::Material*>& aMaterials);
	
	void setIsLiveRender(bool liveRender) { m_isLiveRender = liveRender; }

protected:
	
	void addObjectToScene(Imagine::Object* pObject, FnKat::FnScenegraphIterator sgIterator);
	
	void registerGeometryInstance(Imagine::GeometryInstance* pGeoInstance);

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth);

	void processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator, bool asSubD);

	void processSpecialisedType(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth);

	Imagine::CompactGeometryInstance* createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD,
																	   const FnKat::GroupAttribute& imagineStatements);
	Imagine::CompactGeometryInstance* createCompactGeometryInstanceFromLocationDiscard(FnKat::FnScenegraphIterator iterator, bool asSubD,
																	   const FnKat::GroupAttribute& imagineStatements);

	Imagine::CompoundObject* createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator, unsigned int baseLevelDepth);

	void createCompoundObjectFromLocationRecursive(FnKat::FnScenegraphIterator iterator, std::vector<Imagine::Object*>& aObjects,
												   unsigned int baseLevelDepth, unsigned int currentDepth);

	InstanceInfo findOrBuildInstanceSourceItem(FnKat::FnScenegraphIterator iterator, const std::string& instanceSourcePath);
	void processInstance(FnKat::FnScenegraphIterator iterator);
	void processInstanceArray(FnKat::FnScenegraphIterator iterator);
	
	void processSphere(FnKat::FnScenegraphIterator iterator);

	void processLight(FnKat::FnScenegraphIterator iterator);

	static unsigned char getRenderVisibilityFlags(const FnKat::GroupAttribute& imagineStatements);
	static void processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Imagine::Object* pObject);

	unsigned int processUVs(FnKat::FloatConstVector& uvlist, std::vector<Imagine::UV>& aUVs);
	
	unsigned int sendObjectID(FnKat::FnScenegraphIterator iterator);
	
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
