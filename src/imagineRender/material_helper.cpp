#include "material_helper.h"

#include <stdio.h>

#ifndef STAND_ALONE
#include "materials/standard_material.h"
#include "materials/glass_material.h"
#include "materials/metal_material.h"
#include "materials/translucent_material.h"
#endif

#include "katana_helpers.h"

MaterialHelper::MaterialHelper() : m_pDefaultMaterial(NULL)
{
	FnKat::StringBuilder tnBuilder;
	tnBuilder.push_back("imagineSurface");
	tnBuilder.push_back("imagineLight");

	m_terminatorNodes = tnBuilder.build();

#ifndef STAND_ALONE
	m_pDefaultMaterial = new StandardMaterial();
#endif
}

FnKat::GroupAttribute MaterialHelper::getMaterialForLocation(FnKat::FnScenegraphIterator iterator) const
{
	return FnKat::RenderOutputUtils::getFlattenedMaterialAttr(iterator, m_terminatorNodes);
}

std::string MaterialHelper::getMaterialHash(const FnKat::GroupAttribute& attribute)
{
	return FnKat::RenderOutputUtils::hashAttr(attribute);
}

Material* MaterialHelper::getExistingMaterial(const std::string& hash) const
{
	std::map<std::string, Material*>::const_iterator itFind = m_aMaterialInstances.find(hash);

	if (itFind == m_aMaterialInstances.end())
		return NULL;

	Material* pMaterial = (*itFind).second;

	return pMaterial;
}

void MaterialHelper::addMaterialInstance(const std::string& hash, Material* pMaterial)
{
	m_aMaterialInstances[hash] = pMaterial;
}

Material* MaterialHelper::createNewMaterial(const std::string& hash, const FnKat::GroupAttribute& attribute)
{
	// only add to map if we created material and attribute had a valid material, otherwise, return default
	// material

	// make sure we've got something useful...
	FnKat::StringAttribute shaderNameAttr = attribute.getChildByName("imagineSurfaceShader");

	// if not, return Default material
	if (!shaderNameAttr.isValid())
		return m_pDefaultMaterial;

	// get params...
	FnKat::GroupAttribute shaderParamsAttr = attribute.getChildByName("imagineSurfaceParams");
	// if we don't have them, the material must have default settings, so just create the default material
	// of that type

	std::string shaderName = shaderNameAttr.getValue(shaderName, false);

	Material* pNewMaterial = NULL;

	if (shaderName == "Standard")
	{
		pNewMaterial = createStandardMaterial(shaderParamsAttr);
	}
	else if (shaderName == "Glass")
	{
		pNewMaterial = createGlassMaterial(shaderParamsAttr);
	}
	else if (shaderName == "Metal")
	{
		pNewMaterial = createMetalMaterial(shaderParamsAttr);
	}
	else if (shaderName == "Translucent")
	{
		pNewMaterial = createTranslucentMaterial(shaderParamsAttr);
	}

	if (pNewMaterial)
		return pNewMaterial;

	return m_pDefaultMaterial;
}

Material* MaterialHelper::createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	StandardMaterial* pNewStandardMaterial = new StandardMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f diffColour = ah.getColourParam("diffuse_col", Colour3f(0.8f, 0.8f, 0.8f));
	pNewStandardMaterial->setDiffuseColour(diffColour);

	// diffuse texture overrides colour if available
	std::string diffTexture = ah.getStringParam("diff_texture");
	if (!diffTexture.empty())
	{
		pNewStandardMaterial->setDiffuseTextureMapPath(diffTexture);
	}

	float diffuseRoughness = ah.getFloatParam("diff_roughness", 0.0f);
	pNewStandardMaterial->setDiffuseRoughness(diffuseRoughness);

	Colour3f specColour = ah.getColourParam("spec_col", Colour3f(0.1f, 0.1f, 0.1f));
	pNewStandardMaterial->setSpecularColour(specColour);

	float specRoughness = ah.getFloatParam("spec_roughness", 0.9f);
	pNewStandardMaterial->setSpecularRoughness(specRoughness);

	float reflection = ah.getFloatParam("reflection", 0.0f);
	pNewStandardMaterial->setReflection(reflection);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.0f);

	int fresnelEnabled = ah.getIntParam("fresnel", 0);
	if (fresnelEnabled)
	{
		pNewStandardMaterial->setFresnelEnabled(true);

		float fresnelCoefficient = ah.getFloatParam("fresnel_coef", 0.0f);
		pNewStandardMaterial->setFresnelCoefficient(fresnelCoefficient);

		if (refractionIndex == 1.0f)
			refractionIndex = 1.4f;
	}

	pNewStandardMaterial->setRefractionIndex(refractionIndex);

	float transparency = ah.getFloatParam("transparency", 0.0f);
	pNewStandardMaterial->setTransparancy(transparency);

	return pNewStandardMaterial;
}

Material* MaterialHelper::createGlassMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	GlassMaterial* pNewMaterial = new GlassMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(0.0f, 0.0f, 0.0f));
	pNewMaterial->setColour(colour);

	float reflection = ah.getFloatParam("reflection", 1.0f);
	pNewMaterial->setReflection(reflection);
	float gloss = ah.getFloatParam("gloss", 1.0f);
	pNewMaterial->setGloss(gloss);
	float transparency = ah.getFloatParam("transparency", 1.0f);
	pNewMaterial->setTransparency(transparency);
	float transmittance = ah.getFloatParam("transmittance", 1.0f);
	pNewMaterial->setTransmittance(transmittance);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.517f);
	pNewMaterial->setRefractionIndex(refractionIndex);

	int fresnel = ah.getIntParam("fresnel", 1);
	pNewMaterial->setFresnel((bool)fresnel);

	return pNewMaterial;
}

Material* MaterialHelper::createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	MetalMaterial* pNewMaterial = new MetalMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(0.9f, 0.9f, 0.9f));
	pNewMaterial->setColour(colour);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.39f);
	pNewMaterial->setRefractionIndex(refractionIndex);
	float k = ah.getFloatParam("k", 4.8f);
	pNewMaterial->setK(k);
	float roughness = ah.getFloatParam("roughness", 0.01f);
	pNewMaterial->setRoughness(roughness);

	return pNewMaterial;
}

Material* MaterialHelper::createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	TranslucentMaterial* pNewMaterial = new TranslucentMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f surfaceColour = ah.getColourParam("surface_col", Colour3f(0.7f, 0.7f, 0.7f));
	pNewMaterial->setSurfaceColour(surfaceColour);

	float surfaceRoughness = ah.getFloatParam("surface_roughness", 0.05f);
	pNewMaterial->setSurfaceRoughness(surfaceRoughness);

	Colour3f innerColour = ah.getColourParam("inner_col", Colour3f(0.4f, 0.4f, 0.4f));
	pNewMaterial->setInnerColour(innerColour);

	float subsurfaceDensity = ah.getFloatParam("subsurface_density", 3.1f);
	pNewMaterial->setSubsurfaceDensity(subsurfaceDensity);
	float samplingSensity = ah.getFloatParam("sampling_density", 0.35f);
	pNewMaterial->setSamplingDensity(samplingSensity);
	float transmittance = ah.getFloatParam("transmittance", 0.6f);
	pNewMaterial->setTransmittance(transmittance);
	float absorption = ah.getFloatParam("absorption_ratio", 0.46f);
	pNewMaterial->setAbsorptionRatio(absorption);

	return pNewMaterial;
}

