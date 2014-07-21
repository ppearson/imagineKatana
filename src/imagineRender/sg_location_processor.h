#ifndef SG_LOCATION_PROCESSOR_H
#define SG_LOCATION_PROCESSOR_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#include <map>
#include <vector>

#include "material_helper.h"
#include "light_helpers.h"

#ifndef STAND_ALONE

#include "materials/material.h"
#include "scene.h"

#endif

class CompactGeometryInstance;
class CompoundObject;

class SGLocationProcessor
{
public:
#ifndef STAND_ALONE
	SGLocationProcessor(Scene& scene);
#else
	SGLocationProcessor();
#endif

	struct InstanceInfo
	{
		bool		m_compound;

		union
		{
			CompactGeometryInstance*	pGeoInstance;
			CompoundObject*				pCompoundObject;
		};
	};

	void setUseCompactGeometry(bool useCG)
	{
		m_useCompactGeometry = useCG;
	}

	void setDeduplicateVertexNormals(bool ddVN)
	{
		m_deduplicateVertexNormals = ddVN;
	}

	void setSpecialiseAssemblies(bool sa)
	{
		m_specialiseAssemblies = sa;
	}

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator);

	void processGeometryPolymeshStandard(FnKat::FnScenegraphIterator iterator);
	void processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator);

	void processAssembly(FnKat::FnScenegraphIterator iterator);

	CompactGeometryInstance* createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator);
	CompoundObject* createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator);
	void createCompoundObjectFromLocationRecursive(FnKat::FnScenegraphIterator iterator, std::vector<Object*>& aObjects);

	void processInstance(FnKat::FnScenegraphIterator iterator);
	void processSphere(FnKat::FnScenegraphIterator iterator);

	void processLight(FnKat::FnScenegraphIterator iterator);

	void getFinalMaterials(std::vector<Material*>& aMaterials);

protected:
	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubd;
	bool				m_useCompactGeometry;
	bool				m_deduplicateVertexNormals;
	bool				m_specialiseAssemblies;

	Scene&				m_scene;

	MaterialHelper		m_materialHelper;
	LightHelpers		m_lightHelper;

	std::map<std::string, InstanceInfo>	m_aInstances;
};

#endif // SG_LOCATION_PROCESSOR_H
