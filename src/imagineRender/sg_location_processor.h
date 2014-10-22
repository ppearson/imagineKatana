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

class StandardGeometryInstance;
class CompactGeometryInstance;
class GeometryInstance;
class CompoundObject;
class Object;

class SGLocationProcessor
{
public:
	SGLocationProcessor(Scene& scene);

	struct InstanceInfo
	{
		bool		m_compound;

		union
		{
			GeometryInstance*			pGeoInstance;
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

	void setTriangleType(unsigned int triangleType)
	{
		m_triangleType = triangleType;
	}

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void getFinalMaterials(std::vector<Material*>& aMaterials);

protected:

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth);

	void processGeometryPolymesh(FnKat::FnScenegraphIterator iterator, bool asSubD);
	void processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator, bool asSubD);

	void processAssembly(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth);

	StandardGeometryInstance* createGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD);
	CompactGeometryInstance* createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD);
	CompoundObject* createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator, unsigned int baseLevelDepth);

	void createCompoundObjectFromLocationRecursive(FnKat::FnScenegraphIterator iterator, std::vector<Object*>& aObjects,
												   unsigned int baseLevelDepth, unsigned int currentDepth);

	void processInstance(FnKat::FnScenegraphIterator iterator);
	void processSphere(FnKat::FnScenegraphIterator iterator);

	void processLight(FnKat::FnScenegraphIterator iterator);

	void processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Object* pObject);

protected:
	// TODO: this is getting a bit silly...
	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubd;
	bool				m_deduplicateVertexNormals;
	bool				m_specialiseAssemblies;
	bool				m_flipT;

	unsigned int		m_triangleType;

	Scene&				m_scene;

	MaterialHelper		m_materialHelper;
	LightHelpers		m_lightHelper;

	std::map<std::string, InstanceInfo>	m_aInstances;
};

#endif // SG_LOCATION_PROCESSOR_H
