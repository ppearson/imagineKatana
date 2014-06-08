#include "material_helper.h"

#include <stdio.h>

#ifndef STAND_ALONE
#include "materials/standard_material.h"
#include "materials/glass_material.h"
#include "materials/metal_material.h"
#include "materials/translucent_material.h"
#endif

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

	Colour3f diffColour = getColourParam(shaderParamsAttr, "diffuse_col", Colour3f(0.8f, 0.8f, 0.8f));
	pNewStandardMaterial->setDiffuseColour(diffColour);

	// diffuse texture overrides colour if available
	std::string diffTexture = getStringParam(shaderParamsAttr, "diff_texture");
	if (!diffTexture.empty())
	{
		pNewStandardMaterial->setDiffuseTextureMapPath(diffTexture);
	}

	float diffuseRoughness = getFloatParam(shaderParamsAttr, "diff_roughness", 0.0f);
	pNewStandardMaterial->setDiffuseRoughness(diffuseRoughness);

	Colour3f specColour = getColourParam(shaderParamsAttr, "spec_col", Colour3f(0.1f, 0.1f, 0.1f));
	pNewStandardMaterial->setSpecularColour(specColour);

	float specRoughness = getFloatParam(shaderParamsAttr, "spec_roughness", 0.9f);
	pNewStandardMaterial->setSpecularRoughness(specRoughness);

	float reflection = getFloatParam(shaderParamsAttr, "reflection", 0.0f);
	pNewStandardMaterial->setReflection(reflection);

	float refractionIndex = getFloatParam(shaderParamsAttr, "refraction_index", 1.0f);

	int fresnelEnabled = getIntParam(shaderParamsAttr, "fresnel", 0);
	if (fresnelEnabled)
	{
		pNewStandardMaterial->setFresnelEnabled(true);

		float fresnelCoefficient = getFloatParam(shaderParamsAttr, "fresnel_coef", 0.0f);
		pNewStandardMaterial->setFresnelCoefficient(fresnelCoefficient);

		if (refractionIndex == 1.0f)
			refractionIndex = 1.4f;
	}

	pNewStandardMaterial->setRefractionIndex(refractionIndex);

	float transparency = getFloatParam(shaderParamsAttr, "transparency", 0.0f);
	pNewStandardMaterial->setTransparancy(transparency);

	return pNewStandardMaterial;
}

Material* MaterialHelper::createGlassMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	GlassMaterial* pNewMaterial = new GlassMaterial();

	Colour3f colour = getColourParam(shaderParamsAttr, "colour", Colour3f(0.0f, 0.0f, 0.0f));
	pNewMaterial->setColour(colour);

	float reflection = getFloatParam(shaderParamsAttr, "reflection", 1.0f);
	pNewMaterial->setReflection(reflection);
	float gloss = getFloatParam(shaderParamsAttr, "gloss", 1.0f);
	pNewMaterial->setGloss(gloss);
	float transparency = getFloatParam(shaderParamsAttr, "transparency", 1.0f);
	pNewMaterial->setTransparency(transparency);
	float transmittance = getFloatParam(shaderParamsAttr, "transmittance", 1.0f);
	pNewMaterial->setTransmittance(transmittance);

	float refractionIndex = getFloatParam(shaderParamsAttr, "refraction_index", 1.517f);
	pNewMaterial->setRefractionIndex(refractionIndex);

	int fresnel = getIntParam(shaderParamsAttr, "fresnel", 1);
	pNewMaterial->setFresnel((bool)fresnel);

	return pNewMaterial;
}

Material* MaterialHelper::createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	MetalMaterial* pNewMaterial = new MetalMaterial();

	Colour3f colour = getColourParam(shaderParamsAttr, "colour", Colour3f(0.9f, 0.9f, 0.9f));
	pNewMaterial->setColour(colour);

	float refractionIndex = getFloatParam(shaderParamsAttr, "refraction_index", 1.39f);
	pNewMaterial->setRefractionIndex(refractionIndex);
	float k = getFloatParam(shaderParamsAttr, "k", 4.8f);
	pNewMaterial->setK(k);
	float roughness = getFloatParam(shaderParamsAttr, "roughness", 0.01f);
	pNewMaterial->setRoughness(roughness);

	return pNewMaterial;
}

Material* MaterialHelper::createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	TranslucentMaterial* pNewMaterial = new TranslucentMaterial();

	Colour3f surfaceColour = getColourParam(shaderParamsAttr, "surface_col", Colour3f(0.7f, 0.7f, 0.7f));
	pNewMaterial->setSurfaceColour(surfaceColour);

	float surfaceRoughness = getFloatParam(shaderParamsAttr, "surface_roughness", 0.05f);
	pNewMaterial->setSurfaceRoughness(surfaceRoughness);

	Colour3f innerColour = getColourParam(shaderParamsAttr, "inner_col", Colour3f(0.4f, 0.4f, 0.4f));
	pNewMaterial->setInnerColour(innerColour);

	float subsurfaceDensity = getFloatParam(shaderParamsAttr, "subsurface_density", 3.1f);
	pNewMaterial->setSubsurfaceDensity(subsurfaceDensity);
	float samplingSensity = getFloatParam(shaderParamsAttr, "sampling_density", 0.35f);
	pNewMaterial->setSamplingDensity(samplingSensity);
	float transmittance = getFloatParam(shaderParamsAttr, "transmittance", 0.6f);
	pNewMaterial->setTransmittance(transmittance);
	float absorption = getFloatParam(shaderParamsAttr, "absorption_ratio", 0.46f);
	pNewMaterial->setAbsorptionRatio(absorption);

	return pNewMaterial;
}


float MaterialHelper::getFloatParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, float defaultValue)
{
	FnKat::FloatAttribute floatAttrib = shaderParamsAttr.getChildByName(name);

	if (!floatAttrib.isValid())
		return defaultValue;

	return floatAttrib.getValue(defaultValue, false);
}

int MaterialHelper::getIntParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, int defaultValue)
{
	FnKat::IntAttribute intAttrib = shaderParamsAttr.getChildByName(name);

	if (!intAttrib.isValid())
		return defaultValue;

	return intAttrib.getValue(defaultValue, false);
}

Colour3f MaterialHelper::getColourParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, const Colour3f& defaultValue)
{
	FnKat::FloatAttribute floatAttrib = shaderParamsAttr.getChildByName(name);

	if (!floatAttrib.isValid())
		return defaultValue;

	if (floatAttrib.getTupleSize() != 3)
		return defaultValue;

	FnKat::FloatConstVector data = floatAttrib.getNearestSample(0);

	Colour3f returnValue;
	returnValue.r = data[0];
	returnValue.g = data[1];
	returnValue.b = data[2];

	return returnValue;
}

std::string MaterialHelper::getStringParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name)
{
	FnKat::StringAttribute stringAttrib = shaderParamsAttr.getChildByName(name);

	if (!stringAttrib.isValid())
		return "";

	return stringAttrib.getValue("", false);
}
