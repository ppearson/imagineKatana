#ifndef MATERIAL_HELPER_H
#define MATERIAL_HELPER_H

#include <map>
#include <vector>
#include <string>

#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnDataBuilder.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#ifdef KAT_V_2
#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#else
#include <RenderOutputUtils/RenderOutputUtils.h>
#endif

#include "colour/colour3f.h"

class Material;

class MaterialHelper
{
public:
	MaterialHelper();

	Material* getOrCreateMaterialForLocation(FnKat::FnScenegraphIterator iterator);

	FnKat::GroupAttribute getMaterialForLocation(FnKat::FnScenegraphIterator iterator) const;

	std::vector<Material*>& getMaterialsVector() { return m_aMaterials; }

	Material* getDefaultMaterial() { return m_pDefaultMaterial; }

protected:
#ifndef KAT_V_2
	static std::string getMaterialHash(const FnKat::GroupAttribute& attribute);
#endif

	// this adds to the instances map itself
	Material* createNewMaterial(const FnKat::GroupAttribute& attribute);

protected:
	static Material* createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr,
											FnKat::GroupAttribute& alphaParamsAttr);
	static Material* createGlassMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createPlasticMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createBrushedMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createMetallicPaintMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createVelvetMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createLuminousMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createWireframeMaterial(const FnKat::GroupAttribute& shaderParamsAttr);

protected:
	FnKat::StringAttribute		m_terminatorNodes;

#ifdef KAT_V_2
	std::map<FnAttribute::Hash, Material*> m_aMaterialInstances; // all materials with hashes
#else
	std::map<std::string, Material*>	m_aMaterialInstances; // all materials with hashes
#endif
	std::vector<Material*>				m_aMaterials; // all material instances in std::vector

	Material*					m_pDefaultMaterial;
};

#endif // MATERIAL_HELPER_H
