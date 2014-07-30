#ifndef SG_LOCATION_PROCESSOR_H
#define SG_LOCATION_PROCESSOR_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#include <map>
#include <vector>

#include "material_helper.h"
#include "light_helpers.h"

#include "materials/material.h"
#include "scene.h"

class CompactGeometryInstance;
class CompoundObject;

class SGLocationProcessor
{
public:
	SGLocationProcessor(Scene& scene);

	struct InstanceInfo
	{
		bool		m_compound;

		union
		{
			CompactGeometryInstance*	pGeoInstance;
			CompoundObject*				pCompoundObject;
		};
	};

	void setEnableSubD(bool enableSubD)
	{
		m_enableSubd = enableSubD;
	}

	void setDeduplicateVertexNormals(bool ddVN)
	{
		m_deduplicateVertexNormals = ddVN;
	}

	void setSpecialiseAssemblies(bool sa)
	{
		m_specialiseAssemblies = sa;
	}

	void setFlipT(bool flipT)
	{
		m_flipT = flipT;
	}

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator);

	void processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator, bool asSubD);

	void processAssembly(FnKat::FnScenegraphIterator iterator);

	CompactGeometryInstance* createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD);
	CompoundObject* createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator);
	void createCompoundObjectFromLocationRecursive(FnKat::FnScenegraphIterator iterator, std::vector<Object*>& aObjects);

	void processInstance(FnKat::FnScenegraphIterator iterator);
	void processSphere(FnKat::FnScenegraphIterator iterator);

	void processLight(FnKat::FnScenegraphIterator iterator);

	void getFinalMaterials(std::vector<Material*>& aMaterials);

protected:
	// TODO: this is getting a bit silly...
	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubd;
	bool				m_deduplicateVertexNormals;
	bool				m_specialiseAssemblies;
	bool				m_flipT;

	Scene&				m_scene;

	MaterialHelper		m_materialHelper;
	LightHelpers		m_lightHelper;

	std::map<std::string, InstanceInfo>	m_aInstances;
};

#endif // SG_LOCATION_PROCESSOR_H
