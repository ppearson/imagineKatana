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

class StandardGeometryInstance;
class CompactGeometryInstance;
class GeometryInstance;
class CompoundObject;
class Object;

class SGLocationProcessor
{
public:
	SGLocationProcessor(Scene& scene, const CreationSettings& creationSettings);

	struct InstanceInfo
	{
		bool		m_compound;

		union
		{
			GeometryInstance*			pGeoInstance;
			CompoundObject*				pCompoundObject;
		};
	};

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void getFinalMaterials(std::vector<Material*>& aMaterials);

protected:

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth);

	void processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator, bool asSubD);

	void processAssembly(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth);

	CompactGeometryInstance* createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD,
																	   const FnKat::GroupAttribute& imagineStatements);
	CompactGeometryInstance* createCompactGeometryInstanceFromLocationDiscard(FnKat::FnScenegraphIterator iterator, bool asSubD,
																	   const FnKat::GroupAttribute& imagineStatements);

	CompoundObject* createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator, unsigned int baseLevelDepth);

	void createCompoundObjectFromLocationRecursive(FnKat::FnScenegraphIterator iterator, std::vector<Object*>& aObjects,
												   unsigned int baseLevelDepth, unsigned int currentDepth);

	void processInstance(FnKat::FnScenegraphIterator iterator);
	void processSphere(FnKat::FnScenegraphIterator iterator);

	void processLight(FnKat::FnScenegraphIterator iterator);

	void processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Object* pObject);

	unsigned int processUVs(FnKat::FloatConstVector& uvlist, std::vector<UV>& aUVs);

protected:
	const CreationSettings&		m_creationSettings;

	Scene&						m_scene;

	MaterialHelper				m_materialHelper;
	LightHelpers				m_lightHelper;

	std::map<std::string, InstanceInfo>	m_aInstances;
};

#endif // SG_LOCATION_PROCESSOR_H
