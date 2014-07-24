#ifndef MATERIAL_HELPER_H
#define MATERIAL_HELPER_H

#include <map>
#include <vector>
#include <string>

#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnDataBuilder.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>
#include <RenderOutputUtils/RenderOutputUtils.h>

#include "colour/colour3f.h"

class Material;

class MaterialHelper
{
public:
	MaterialHelper();

	FnKat::GroupAttribute getMaterialForLocation(FnKat::FnScenegraphIterator iterator) const;

	static std::string getMaterialHash(const FnKat::GroupAttribute& attribute);

	Material* getExistingMaterial(const std::string& hash) const;

	void addMaterialInstance(const std::string& hash, Material* pMaterial);

	// this adds to the instances map itself
	Material* createNewMaterial(const std::string& hash, const FnKat::GroupAttribute& attribute);

	std::vector<Material*>& getMaterialsVector() { return m_aMaterials; }

protected:
	static Material* createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createGlassMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createBrushedMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createMetallicPaintMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr);

protected:
	FnKat::StringAttribute m_terminatorNodes;

	std::map<std::string, Material*>	m_aMaterialInstances; // all materials with hashes
	std::vector<Material*>				m_aMaterials; // all material instances in std::vector

	Material*		m_pDefaultMaterial;
};

#endif // MATERIAL_HELPER_H
