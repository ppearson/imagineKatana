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
}

class IDState;

class SGLocationProcessor
{
public:
	SGLocationProcessor(Imagine::Scene& scene, const CreationSettings& creationSettings);
	~SGLocationProcessor();

	struct InstanceInfo
	{
		InstanceInfo() : m_compound(false), pGeoInstance(NULL)
		{
			
		}

		bool		m_compound;

		union
		{
			Imagine::GeometryInstance*			pGeoInstance;
			Imagine::CompoundObject*			pCompoundObject;
		};
	};
	
	void initIDState(const std::string& hostName, int64_t frameID);

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void getFinalMaterials(std::vector<Imagine::Material*>& aMaterials);

protected:
	
	void addObjectToScene(Imagine::Object* pObject);
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

	static void processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Imagine::Object* pObject);

	unsigned int processUVs(FnKat::FloatConstVector& uvlist, std::vector<Imagine::UV>& aUVs);
	
	unsigned int sendObjectID(FnKat::FnScenegraphIterator iterator);
	
	unsigned int getCustomGeoFlags();

protected:
	const CreationSettings&		m_creationSettings;

	Imagine::Scene&				m_scene;

	MaterialHelper				m_materialHelper;
	LightHelpers				m_lightHelper;

	std::map<std::string, InstanceInfo>	m_aInstances;
	
	IDState*					m_pIDState;
};

#endif // SG_LOCATION_PROCESSOR_H
