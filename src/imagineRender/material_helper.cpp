#include "material_helper.h"

#include <stdio.h>

#include "materials/standard_material.h"
#include "materials/glass_material.h"
#include "materials/metal_material.h"
#include "materials/translucent_material.h"
#include "materials/brushed_metal_material.h"
#include "materials/metalic_paint_material.h"
#include "materials/velvet_material.h"
#include "materials/wireframe_material.h"

#include "katana_helpers.h"

MaterialHelper::MaterialHelper() : m_pDefaultMaterial(NULL)
{
	FnKat::StringBuilder tnBuilder;
	tnBuilder.push_back("imagineSurface");
	tnBuilder.push_back("imagineLight");
	tnBuilder.push_back("imagineBump");
	tnBuilder.push_back("imagineAlpha");

	m_terminatorNodes = tnBuilder.build();

	m_pDefaultMaterial = new StandardMaterial();
}

Material* MaterialHelper::getOrCreateMaterialForLocation(FnKat::FnScenegraphIterator iterator)
{
	FnKat::GroupAttribute materialAttrib = getMaterialForLocation(iterator);

	Material* pMaterial = NULL;

#ifdef KAT_V_2
	FnAttribute::Hash materialHash = materialAttrib.getHash();

	std::map<FnAttribute::Hash, Material*>::const_iterator itFind = m_aMaterialInstances.find(materialHash);
#else
	// calculate a hash for the material
	std::string materialHash = getMaterialHash(materialAttrib);

	// see if we've got it already
	std::map<std::string, Material*>::const_iterator itFind = m_aMaterialInstances.find(materialHash);
#endif
	if (itFind != m_aMaterialInstances.end())
	{
		pMaterial = (*itFind).second;

		if (pMaterial)
			return pMaterial;
	}

	// otherwise, create a new material
	pMaterial = createNewMaterial(materialAttrib);

	// add this new material to our list of material instances
	m_aMaterialInstances[materialHash] = pMaterial;
	m_aMaterials.push_back(pMaterial);

	return pMaterial;
}

FnKat::GroupAttribute MaterialHelper::getMaterialForLocation(FnKat::FnScenegraphIterator iterator) const
{
	return FnKat::RenderOutputUtils::getFlattenedMaterialAttr(iterator, m_terminatorNodes);
}

#ifndef KAT_V_2
std::string MaterialHelper::getMaterialHash(const FnKat::GroupAttribute& attribute)
{
	return FnKat::RenderOutputUtils::hashAttr(attribute);
}
#endif

Material* MaterialHelper::createNewMaterial(const FnKat::GroupAttribute& attribute)
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

	std::string shaderName = shaderNameAttr.getValue("", false);

	// see if we've got any Bump or Alpha materials - this isn't great doing it this way,
	// as Imagine's versions are (possibly wrongly) part of the below shader types, but it's the only way
	// currently
	FnKat::GroupAttribute bumpParamsAttr = attribute.getChildByName("imagineBumpParams");
	FnKat::GroupAttribute alphaParamsAttr = attribute.getChildByName("imagineAlphaParams");

	Material* pNewMaterial = NULL;

	if (shaderName == "Standard" || shaderName == "StandardImage")
	{
		pNewMaterial = createStandardMaterial(shaderParamsAttr, bumpParamsAttr, alphaParamsAttr);
	}
	else if (shaderName == "Glass")
	{
		pNewMaterial = createGlassMaterial(shaderParamsAttr);
	}
	else if (shaderName == "Metal")
	{
		pNewMaterial = createMetalMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (shaderName == "Brushed Metal")
	{
		pNewMaterial = createBrushedMetalMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (shaderName == "Metallic Paint")
	{
		pNewMaterial = createMetallicPaintMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (shaderName == "Translucent")
	{
		pNewMaterial = createTranslucentMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (shaderName == "Velvet")
	{
		pNewMaterial = createVelvetMaterial(shaderParamsAttr);
	}
	else if (shaderName == "Wireframe")
	{
		pNewMaterial = createWireframeMaterial(shaderParamsAttr);
	}

	if (pNewMaterial)
		return pNewMaterial;

	return m_pDefaultMaterial;
}

Material* MaterialHelper::createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr,
												 FnKat::GroupAttribute& alphaParamsAttr)
{
	StandardMaterial* pNewStandardMaterial = new StandardMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f diffColour = ah.getColourParam("diff_col", Colour3f(0.8f, 0.8f, 0.8f));
	pNewStandardMaterial->setDiffuseColour(diffColour);

	// diffuse texture overrides colour if available
	std::string diffColTexture = ah.getStringParam("diff_col_texture");
	if (!diffColTexture.empty())
	{
		pNewStandardMaterial->setDiffuseTextureMapPath(diffColTexture, true); // lazy load texture when needed

		int diffTextureFlags = ah.getIntParam("diff_col_texture_flags", 0);
		if (diffTextureFlags)
		{
			// hack mip-map bias for the moment
			if (diffTextureFlags == 1)
			{
				pNewStandardMaterial->setDiffuseTextureCustomFlags(1 << 15);
			}
			else if (diffTextureFlags == 2)
			{
				pNewStandardMaterial->setDiffuseTextureCustomFlags(1 << 16);
			}
		}
	}

	float diffRoughness = ah.getFloatParam("diff_roughness", 0.0f);
	pNewStandardMaterial->setDiffuseRoughness(diffRoughness);

	std::string diffRoughnessTexture = ah.getStringParam("diff_roughness_texture");
	if (!diffRoughnessTexture.empty())
	{
		pNewStandardMaterial->setDiffuseRoughnessTextureMapPath(diffRoughnessTexture, true);
	}

	float diffBacklit = ah.getFloatParam("diff_backlit", 0.0f);
	pNewStandardMaterial->setDiffuseBacklit(diffBacklit);

	std::string diffBacklitTexture = ah.getStringParam("diff_backlit_texture");
	if (!diffBacklitTexture.empty())
	{
		pNewStandardMaterial->setDiffuseBacklitTextureMapPath(diffBacklitTexture, true);
	}

	Colour3f specColour = ah.getColourParam("spec_col", Colour3f(0.1f, 0.1f, 0.1f));
	pNewStandardMaterial->setSpecularColour(specColour);

	// specular texture overrides colour if available
	std::string specTexture = ah.getStringParam("spec_col_texture");
	if (!specTexture.empty())
	{
		pNewStandardMaterial->setSpecularTextureMapPath(specTexture, true); // lazy load texture when needed

		int specTextureFlags = ah.getIntParam("spec_col_texture_flags", 0);
		if (specTextureFlags)
		{
			// hack mip-map bias for the moment
			if (specTextureFlags == 1)
			{
				pNewStandardMaterial->setSpecularTextureCustomFlags(1 << 15);
			}
			else if (specTextureFlags == 2)
			{
				pNewStandardMaterial->setSpecularTextureCustomFlags(1 << 16);
			}
		}
	}

	float specRoughness = ah.getFloatParam("spec_roughness", 0.9f);
	pNewStandardMaterial->setSpecularRoughness(specRoughness);

	std::string specRoughnessTexture = ah.getStringParam("spec_roughness_texture");
	if (!specRoughnessTexture.empty())
	{
		pNewStandardMaterial->setSpecularRoughnessTextureMapPath(specRoughnessTexture, true);
	}

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

	int doubleSided = ah.getIntParam("double_sided", 0);
	if (doubleSided == 1)
	{
		pNewStandardMaterial->setDoubleSided(true);
	}

	if (bumpParamsAttr.isValid())
	{
		KatanaAttributeHelper ahBump(bumpParamsAttr);

		std::string bumpTexture = ahBump.getStringParam("bump_texture_path");
		if (!bumpTexture.empty())
		{
			pNewStandardMaterial->setBumpTextureMapPath(bumpTexture, true);

			float bumpIntensity = ahBump.getFloatParam("bump_texture_intensity", 0.8f);
			pNewStandardMaterial->setBumpIntensity(bumpIntensity);
		}
	}

	if (alphaParamsAttr.isValid())
	{
		KatanaAttributeHelper ahAlpha(alphaParamsAttr);

		std::string alphaTexture = ahAlpha.getStringParam("alpha_texture_path");
		if (!alphaTexture.empty())
		{
			pNewStandardMaterial->setAlphaTextureMapPath(alphaTexture, true);

			int invertTexture = ahAlpha.getIntParam("alpha_texture_invert", 0);
			if (invertTexture == 1)
			{
				pNewStandardMaterial->setAlphaTextureInvert(true);
			}
		}
	}

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

	int ignoreRefraction = ah.getIntParam("ignore_trans_refraction", 0);
	pNewMaterial->setIgnoreTransmissionRefraction((bool)ignoreRefraction);

	return pNewMaterial;
}

Material* MaterialHelper::createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr)
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

	int doubleSided = ah.getIntParam("double_sided", 0);
	if (doubleSided == 1)
	{
		pNewMaterial->setDoubleSided(true);
	}

	if (bumpParamsAttr.isValid())
	{
		KatanaAttributeHelper ahBump(bumpParamsAttr);

		std::string bumpTexture = ahBump.getStringParam("bump_texture_path");
		if (!bumpTexture.empty())
		{
			pNewMaterial->setBumpTextureMapPath(bumpTexture, true);

			float bumpIntensity = ahBump.getFloatParam("bump_texture_intensity", 0.8f);
			pNewMaterial->setBumpIntensity(bumpIntensity);
		}
	}

	return pNewMaterial;
}

Material* MaterialHelper::createBrushedMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr)
{
	BrushedMetalMaterial* pNewMaterial = new BrushedMetalMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(0.9f, 0.9f, 0.9f));
	pNewMaterial->setColour(colour);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.39f);
	pNewMaterial->setRefractionIndex(refractionIndex);
	float k = ah.getFloatParam("k", 4.8f);
	pNewMaterial->setK(k);
	float roughnessX = ah.getFloatParam("roughness_x", 0.1f);
	pNewMaterial->setRoughnessX(roughnessX);
	float roughnessY = ah.getFloatParam("roughness_y", 0.02f);
	pNewMaterial->setRoughnessY(roughnessY);

	int doubleSided = ah.getIntParam("double_sided", 0);
	if (doubleSided == 1)
	{
		pNewMaterial->setDoubleSided(true);
	}

	if (bumpParamsAttr.isValid())
	{
		KatanaAttributeHelper ahBump(bumpParamsAttr);

		std::string bumpTexture = ahBump.getStringParam("bump_texture_path");
		if (!bumpTexture.empty())
		{
			pNewMaterial->setBumpTextureMapPath(bumpTexture, true);

			float bumpIntensity = ahBump.getFloatParam("bump_texture_intensity", 0.8f);
			pNewMaterial->setBumpIntensity(bumpIntensity);
		}
	}

	return pNewMaterial;
}

Material* MaterialHelper::createMetallicPaintMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr)
{
	MetallicPaintMaterial* pNewMaterial = new MetallicPaintMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(0.29f, 0.016f, 0.019f));
	pNewMaterial->setColour(colour);

	Colour3f flakeColour = ah.getColourParam("flake_colour", Colour3f(0.39, 0.016f, 0.19f));
	pNewMaterial->setFlakeColour(flakeColour);

	float flakeSpread = ah.getFloatParam("flake_spread", 0.32f);
	pNewMaterial->setFlakeSpread(flakeSpread);

	float flakeMix = ah.getFloatParam("flake_mix", 0.38f);
	pNewMaterial->setFlakeMix(flakeMix);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.39f);
	pNewMaterial->setRefractionIndex(refractionIndex);

	float reflection = ah.getFloatParam("reflection", 1.0f);
	pNewMaterial->setReflection(reflection);

	float fresnelCoefficient = ah.getFloatParam("fresnel_coef", 0.0f);
	pNewMaterial->setFresnelCoefficient(fresnelCoefficient);

	if (bumpParamsAttr.isValid())
	{
		KatanaAttributeHelper ahBump(bumpParamsAttr);

		std::string bumpTexture = ahBump.getStringParam("bump_texture_path");
		if (!bumpTexture.empty())
		{
			pNewMaterial->setBumpTextureMapPath(bumpTexture, true);

			float bumpIntensity = ahBump.getFloatParam("bump_texture_intensity", 0.8f);
			pNewMaterial->setBumpIntensity(bumpIntensity);
		}
	}

	return pNewMaterial;
}

Material* MaterialHelper::createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr)
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

	if (bumpParamsAttr.isValid())
	{
		KatanaAttributeHelper ahBump(bumpParamsAttr);

		std::string bumpTexture = ahBump.getStringParam("bump_texture_path");
		if (!bumpTexture.empty())
		{
			pNewMaterial->setBumpTextureMapPath(bumpTexture, true);

			float bumpIntensity = ahBump.getFloatParam("bump_texture_intensity", 0.8f);
			pNewMaterial->setBumpIntensity(bumpIntensity);
		}
	}

	return pNewMaterial;
}

Material* MaterialHelper::createVelvetMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	VelvetMaterial* pNewMaterial = new VelvetMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f horizonColour = ah.getColourParam("horiz_col", Colour3f(0.7f, 0.7f, 0.7f));
	pNewMaterial->setHorizonScatteringColour(horizonColour);

	std::string horizonColourTexture = ah.getStringParam("horiz_col_texture");
	if (!horizonColourTexture.empty())
	{
		pNewMaterial->setHorizonScatteringColourTexture(horizonColourTexture, true);
	}

	float horizonScatterFalloff = ah.getFloatParam("horiz_scatter_falloff", 0.4f);
	pNewMaterial->setHorizonScatteringFalloff(horizonScatterFalloff);

	Colour3f backscatterColour = ah.getColourParam("backscatter_col", Colour3f(0.4f, 0.4f, 0.4f));
	pNewMaterial->setBackScatteringColour(backscatterColour);

	std::string backscatterColourTexture = ah.getStringParam("backscatter_col_texture");
	if (!backscatterColourTexture.empty())
	{
		pNewMaterial->setBackScatteringColourTexture(backscatterColourTexture, true);
	}

	float backscatterFalloff = ah.getFloatParam("backscatter", 0.7f);
	pNewMaterial->setBackScatteringFalloff(backscatterFalloff);

	return pNewMaterial;
}

Material* MaterialHelper::createWireframeMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	WireframeMaterial* pNewMaterial = new WireframeMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f interiorColour = ah.getColourParam("interior_colour", Colour3f(0.7f, 0.7f, 0.7f));
	pNewMaterial->setInteriorColour(interiorColour);

	float lineWidth = ah.getFloatParam("line_width", 0.005f);
	pNewMaterial->setLineWidth(lineWidth);

	Colour3f lineColour = ah.getColourParam("line_colour", Colour3f(0.01f, 0.01f, 0.01f));
	pNewMaterial->setLineColour(lineColour);

	float edgeSoftness = ah.getFloatParam("edge_softness", 0.3f);
	pNewMaterial->setEdgeSoftness(edgeSoftness);

	int edgeType = ah.getIntParam("edge_type", 1);
	pNewMaterial->setEdgeType(edgeType);

	return pNewMaterial;
}

