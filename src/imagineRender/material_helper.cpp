#include "material_helper.h"

#include <stdio.h>

#include "materials/standard_material.h"
#include "materials/glass_material.h"
#include "materials/metal_material.h"
#include "materials/plastic_material.h"
#include "materials/translucent_material.h"
#include "materials/brushed_metal_material.h"
#include "materials/metalic_paint_material.h"
#include "materials/velvet_material.h"
#include "materials/luminous_material.h"

#include "textures/ops/op_invert.h"
#include "textures/image/image_texture_lazy.h"
#include "textures/image/image_texture_lazy_atlas.h"

#include "textures/constant.h"
#include "textures/procedural_2d/wireframe.h"

#include "katana_helpers.h"

using namespace Imagine;

MaterialHelper::MaterialHelper() : m_pDefaultMaterial(NULL), m_pDefaultMaterialMatte(NULL)
{
	FnKat::StringBuilder tnBuilder;
	tnBuilder.push_back("imagineSurface");
	tnBuilder.push_back("imagineLight");
	tnBuilder.push_back("imagineBump");
	tnBuilder.push_back("imagineAlpha");

	m_terminatorNodes = tnBuilder.build();

	// TODO: these leak, although only once per session...
	m_pDefaultMaterial = new StandardMaterial();
	m_pDefaultMaterialMatte = new StandardMaterial();
	m_pDefaultMaterialMatte->setMatte(true);
}

Material* MaterialHelper::getOrCreateMaterialForLocation(FnKat::FnScenegraphIterator iterator, const FnKat::GroupAttribute& imagineStatements)
{
	FnKat::GroupAttribute materialAttrib = getMaterialForLocation(iterator);

	Material* pMaterial = NULL;

	// currently, Imagine controls whether objects are Matte from materials, so we need to inject the matte state
	// into the Material's hash... In the future, this is likely to change and Matte will become a full object attribute/flag...

	bool isMatte = false;
	FnKat::IntAttribute matteAttribute = imagineStatements.getChildByName("matte");
	if (matteAttribute.isValid())
	{
		isMatte = matteAttribute.getValue(0, false) == 1;
	}

#ifdef KAT_V_2
	FnAttribute::Hash materialRawHash = materialAttrib.getHash();
	Hash hash;
	hash.addLongLong(materialRawHash.uint64());
	hash.addUChar((unsigned char)isMatte);
	HashValue materialHash = hash.getHash();

	std::map<HashValue, Material*>::const_iterator itFind = m_aMaterialInstances.find(materialHash);
#else
	// calculate a hash for the material
	std::string materialHashRaw = getMaterialHash(materialAttrib);

	Hash hash;
	hash.addString(materialHashRaw);
	hash.addUChar((unsigned char)isMatte);
	HashValue materialHash = hash.getHash();

	// see if we've got it already
	std::map<HashValue, Material*>::const_iterator itFind = m_aMaterialInstances.find(materialHash);
#endif
	if (itFind != m_aMaterialInstances.end())
	{
		pMaterial = (*itFind).second;

		if (pMaterial)
			return pMaterial;
	}

	// otherwise, create a new material
	pMaterial = createNewMaterial(materialAttrib, isMatte);

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

Material* MaterialHelper::createNewMaterial(const FnKat::GroupAttribute& attribute, bool isMatte)
{
	// only add to map if we created material and attribute had a valid material, otherwise, return default
	// material

	// make sure we've got something useful...
	FnKat::StringAttribute shaderNameAttr = attribute.getChildByName("imagineSurfaceShader");

	Material* pNewMaterial = NULL;

	// if we haven't got a standard material item, see if we've got a network material instead...
	if (!shaderNameAttr.isValid())
	{
		// try and create a network material from the attribute
		pNewMaterial = createNetworkMaterial(attribute, isMatte);

		// if we haven't, then just return the default material
		if (!pNewMaterial)
		{
			return isMatte ? m_pDefaultMaterialMatte : m_pDefaultMaterial;
		}
	}

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
	else if (shaderName == "Plastic")
	{
		pNewMaterial = createPlasticMaterial(shaderParamsAttr, bumpParamsAttr);
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
	else if (shaderName == "Luminous")
	{
		pNewMaterial = createLuminousMaterial(shaderParamsAttr);
	}

	if (isMatte && pNewMaterial)
	{
		pNewMaterial->setMatte(true);
	}

	if (pNewMaterial)
		return pNewMaterial;

	return m_pDefaultMaterial;
}

// TODO: this whole infrastructure for handling Network Materials is pretty hacky...
Material* MaterialHelper::createNetworkMaterial(const FnKat::GroupAttribute& attribute, bool isMatte)
{
	FnKat::GroupAttribute nodesAttr = attribute.getChildByName("nodes");
	FnKat::GroupAttribute terminalsAttr = attribute.getChildByName("terminals");

	if (!nodesAttr.isValid() || !terminalsAttr.isValid())
	{
		return NULL;
	}

	std::map<std::string, Material*> aMaterialNodes; // there should really only be one of these, but...
	std::map<std::string, Texture*> aOpNodes;
	std::map<std::string, Texture*> aTextureNodes;
	
	Material* pNewNodeMaterial = NULL;

	// process all node items and cache them by name...
	unsigned int numNodes = nodesAttr.getNumberOfChildren();
	for (unsigned int i = 0; i < numNodes; i++)
	{
		FnKat::GroupAttribute subItem = nodesAttr.getChildByIndex(i);

		KatanaAttributeHelper ah(subItem);

		std::string nodeName = ah.getStringParam("name", "");
		std::string nodeType = ah.getStringParam("type", "");

		bool isShader = nodeType.find("Shader/") != std::string::npos;
		bool isOp = nodeType.find("Op/") != std::string::npos;
		bool isTexture = nodeType.find("Texture/") != std::string::npos;

		if (!isShader && !isOp && !isTexture)
		{
			// if we don't know what it is, there's no point continuing with the other nodes...
			fprintf(stderr, "Error processing NetworkMaterial - unknown node type: %s\n", nodeType.c_str());
			return NULL;
		}

		FnKat::GroupAttribute paramsAttr = subItem.getChildByName("parameters");

		if (isShader)
		{
			std::string shaderName = nodeType.substr(7);

			FnKat::GroupAttribute bumpParamsAttr;
			FnKat::GroupAttribute alphaParamsAttr;

			// TODO: duplicate of above code, but...
			if (shaderName == "Standard" || shaderName == "StandardImage")
			{
				pNewNodeMaterial = createStandardMaterial(paramsAttr, bumpParamsAttr, alphaParamsAttr);
			}
			else if (shaderName == "Glass")
			{
				pNewNodeMaterial = createGlassMaterial(paramsAttr);
			}
			else if (shaderName == "Metal")
			{
				pNewNodeMaterial = createMetalMaterial(paramsAttr, bumpParamsAttr);
			}
			else if (shaderName == "Plastic")
			{
				pNewNodeMaterial = createPlasticMaterial(paramsAttr, bumpParamsAttr);
			}
			else if (shaderName == "Metallic Paint")
			{
				pNewNodeMaterial = createMetallicPaintMaterial(paramsAttr, bumpParamsAttr);
			}
			else if (shaderName == "Translucent")
			{
				pNewNodeMaterial = createTranslucentMaterial(paramsAttr, bumpParamsAttr);
			}

			if (!pNewNodeMaterial)
				continue;

			aMaterialNodes[nodeName] = pNewNodeMaterial;
		}
		else if (isOp)
		{
			std::string opName = nodeType.substr(3);

			Texture* pNewNodeOp = createNetworkOpItem(opName, paramsAttr);
			if (!pNewNodeOp)
				continue;

			aOpNodes[nodeName] = pNewNodeOp;
		}
		else if (isTexture)
		{
			std::string textureName = nodeType.substr(8);
			
			Texture* pNewNodeTexture = createNetworkTextureItem(textureName, paramsAttr);
			if (!pNewNodeTexture)
				continue;

			aTextureNodes[nodeName] = pNewNodeTexture;
		}
	}
	
	if (!pNewNodeMaterial)
		return NULL;

	// now that we've gone through all nodes and created them, we can go through again, looking for
	// connections, and connect them up
	for (unsigned int i = 0; i < numNodes; i++)
	{
		FnKat::GroupAttribute subItem = nodesAttr.getChildByIndex(i);

		KatanaAttributeHelper ah(subItem);

		FnKat::GroupAttribute connectionsAttr = subItem.getChildByName("connections");
		if (!connectionsAttr.isValid())
			continue;

		std::string nodeName = ah.getStringParam("name", "");
		std::string nodeType = ah.getStringParam("type", "");

		bool isShader = nodeType.find("Shader/") != std::string::npos;
//		bool isOp = nodeType.find("Op/") != std::string::npos;

		unsigned int numConnections = connectionsAttr.getNumberOfChildren();

		if (isShader)
		{
			// we're connecting a Texture Op into this shader as an input
			std::string shaderName = nodeType.substr(7);

			Material* pMaterial = aMaterialNodes[nodeName];

			for (unsigned int j = 0; j < numConnections; j++)
			{
				std::string paramName = connectionsAttr.getChildName(j);

				FnKat::StringAttribute connItemAttr = connectionsAttr.getChildByIndex(j);

				if (connItemAttr.isValid() && connItemAttr.getValue("", false).find("@") != std::string::npos)
				{
					std::string connectedItem = connItemAttr.getValue("", false);

					size_t atPos = connectedItem.find("@");

					std::string portName = connectedItem.substr(0, atPos);
					std::string connectionNodeName = connectedItem.substr(atPos + 1);
					
					bool foundConnection = false;

					// now find what it's connected to
					// for the moment, hopefully we're only going to be connecting Ops (Textures)

					std::map<std::string, Texture*>::const_iterator itFindItem = aOpNodes.find(connectionNodeName);
					if (itFindItem != aOpNodes.end())
					{
						const Texture* pConn = itFindItem->second;
						connectOpToMaterial(pMaterial, shaderName, paramName, pConn);
						continue;
					}
					
					// otherwise, see if it was a texture
					itFindItem = aTextureNodes.find(connectionNodeName);
					if (itFindItem != aTextureNodes.end())
					{
						const Texture* pConn = itFindItem->second;
						connectTextureToMaterial(pMaterial, shaderName, paramName, pConn);
						continue;
					}
					
					// otherwise, we didn't find it..
					fprintf(stderr, "Can't find existing Node: %s for connection: %s\n", connectionNodeName.c_str(), paramName.c_str());
					continue;
				}
				else
				{
					fprintf(stderr, "Invalid connection item...\n");
				}
			}
		}
		else
		{
			// We're connecting something (hopefully another Texture Op) into this texture op
			Texture* pTargetTexture = aOpNodes[nodeName];

			// we're connecting a Texture Op into this shader as an input
			std::string opName = nodeType.substr(3);

			for (unsigned int j = 0; j < numConnections; j++)
			{
				std::string paramName = connectionsAttr.getChildName(j);

				FnKat::StringAttribute connItemAttr = connectionsAttr.getChildByIndex(j);

				if (connItemAttr.isValid() && connItemAttr.getValue("", false).find("@") != std::string::npos)
				{
					std::string connectedItem = connItemAttr.getValue("", false);

					size_t atPos = connectedItem.find("@");

					std::string portName = connectedItem.substr(0, atPos);
					std::string connectionNodeName = connectedItem.substr(atPos + 1);

					// now find what it's connected to
					// for the moment, hopefully we're only going to be connecting Ops (Textures)

					std::map<std::string, Texture*>::const_iterator itFindItem = aOpNodes.find(connectionNodeName);

					if (itFindItem == aOpNodes.end())
					{
						fprintf(stderr, "Can't find existing Node: %s for connection: %s\n", connectionNodeName.c_str(), paramName.c_str());
						continue;
					}

					const Texture* pConn = itFindItem->second;

					connectOpToOp(pTargetTexture, opName, paramName, pConn);
				}
				else
				{
					fprintf(stderr, "Invalid connection item...\n");
				}
			}
		}
	}

	// now go through terminals connecting them up...
	// for the moment, we can just do surface ones...

	return pNewNodeMaterial;
}

// TODO: these are increadibly hacky - this sort of infrastructure should be moved into Imagine properly and be
//       passed in as generic key/value attributes

Texture* MaterialHelper::createNetworkOpItem(const std::string& opName, const FnKat::GroupAttribute& params)
{
	// TODO: do something better than this...

	if (opName == "Mix")
	{

	}
	return NULL;
}

Texture* MaterialHelper::createNetworkTextureItem(const std::string& textureName, const FnKat::GroupAttribute& params)
{
	// TODO: do something better than this...
	
	Texture* pNewTexture = NULL;

	if (textureName == "TextureRead")
	{

	}
	else if (textureName == "Constant")
	{
		pNewTexture = new Constant(Colour3f(1.0f, 0.0f, 0.0f));
	}
	else if (textureName == "Wireframe")
	{

	}
	return pNewTexture;
}

void MaterialHelper::connectOpToMaterial(Material* pMaterial, const std::string& shaderName, const std::string& paramName, const Texture* pOp)
{
	if (shaderName == "Standard" || shaderName == "StandardImage")
	{
		StandardMaterial* pSM = static_cast<StandardMaterial*>(pMaterial);

		if (paramName == "diff_col")
		{
			pSM->setDiffuseColourManualTexture(pOp);
		}
	}
}

void MaterialHelper::connectTextureToMaterial(Material* pMaterial, const std::string& shaderName, const std::string& paramName, const Texture* pTexture)
{
	if (shaderName == "Standard" || shaderName == "StandardImage")
	{
		StandardMaterial* pSM = static_cast<StandardMaterial*>(pMaterial);

		if (paramName == "diff_col")
		{
			pSM->setDiffuseColourManualTexture(pTexture);
		}
		else if (paramName == "spec_col")
		{
//			pSM->setSpecularColourManualTexture(pTexture);
		}
	}
}

void MaterialHelper::connectOpToOp(Texture* pTargetOp, const std::string& opName, const std::string& paramName, const Texture* pSourceOp)
{

}

Material* MaterialHelper::createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr,
												 FnKat::GroupAttribute& alphaParamsAttr)
{
	StandardMaterial* pNewStandardMaterial = new StandardMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f diffColour = ah.getColourParam("diff_col", Colour3f(0.6f, 0.6f, 0.6f));
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

	Colour3f specColour = ah.getColourParam("spec_col", Colour3f(0.0f, 0.0f, 0.0f));
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

	float specRoughness = ah.getFloatParam("spec_roughness", 0.15f);
	pNewStandardMaterial->setSpecularRoughness(specRoughness);

	std::string specRoughnessTexture = ah.getStringParam("spec_roughness_texture");
	if (!specRoughnessTexture.empty())
	{
		pNewStandardMaterial->setSpecularRoughnessTextureMapPath(specRoughnessTexture, true);
	}

	pNewStandardMaterial->setSpecularType(0);

	float reflection = ah.getFloatParam("reflection", 0.0f);
	pNewStandardMaterial->setReflection(reflection);

	float reflectionRoughness = ah.getFloatParam("reflection_roughness", 0.0f);
	pNewStandardMaterial->setReflectionRoughness(reflectionRoughness);

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
	float roughness = ah.getFloatParam("roughness", 0.0f);
	pNewMaterial->setRoughness(roughness);
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

	int thinVolume = ah.getIntParam("thin_volume", 0);
	pNewMaterial->setThinVolume((bool)thinVolume);

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

Material* MaterialHelper::createPlasticMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr)
{
	PlasticMaterial* pNewMaterial = new PlasticMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(0.5f, 0.5f, 1.0f));
	pNewMaterial->setColour(colour);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.39f);
	pNewMaterial->setRefractionIndex(refractionIndex);

	float roughness = ah.getFloatParam("roughness", 0.01f);
	pNewMaterial->setRoughness(roughness);

	float fresnelCoefficient = ah.getFloatParam("fresnel_coef", 0.0f);
	pNewMaterial->setFresnelCoefficient(fresnelCoefficient);
/*
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
*/
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

	std::string colourTexture = ah.getStringParam("col_texture");
	if (!colourTexture.empty())
	{
		pNewMaterial->setColourTexture(colourTexture, true); // lazy load texture when needed
	}

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

	Colour3f specularColour = ah.getColourParam("specular_col", Colour3f(0.1f, 0.1f, 0.1f));
	pNewMaterial->setSpecularColour(specularColour);

	float surfaceRoughness = ah.getFloatParam("specular_roughness", 0.05f);
	pNewMaterial->setSpecularRoughness(surfaceRoughness);

	Colour3f innerColour = ah.getColourParam("inner_col", Colour3f(0.4f, 0.4f, 0.4f));
	pNewMaterial->setInnerColour(innerColour);

	float subsurfaceDensity = ah.getFloatParam("subsurface_density", 3.1f);
	pNewMaterial->setSubsurfaceDensity(subsurfaceDensity);
	float samplingSensity = ah.getFloatParam("sampling_density", 0.35f);
	pNewMaterial->setSamplingDensity(samplingSensity);

	float transmittance = ah.getFloatParam("transmittance", 0.41f);
	pNewMaterial->setTransmittance(transmittance);
	float transmittanceRoughness = ah.getFloatParam("transmittance_roughness", 0.33f);
	pNewMaterial->setTransmittanceRoughness(transmittanceRoughness);

	float absorption = ah.getFloatParam("absorption_ratio", 0.46f);
	pNewMaterial->setAbsorptionRatio(absorption);

	float refractionIndex = ah.getFloatParam("refractionIndex", 1.42f);
	pNewMaterial->setRefractionIndex(refractionIndex);

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

Material* MaterialHelper::createLuminousMaterial(const FnKat::GroupAttribute& shaderParamsAttr)
{
	LuminousMaterial* pNewMaterial = new LuminousMaterial();

	KatanaAttributeHelper ah(shaderParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f, 1.0f, 1.0f));
	pNewMaterial->setColour(colour);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	pNewMaterial->setIntensity(intensity);

	int registerAsLight = ah.getIntParam("register_as_light", 0);

	if (registerAsLight == 1)
	{
		pNewMaterial->setRegisterAsLight(true);

		float lightIntensity = ah.getFloatParam("light_intensity", 1.0f);
		float lightExposure = ah.getFloatParam("light_exposure", 3.0f);

		float fullIntensity = lightIntensity * std::pow(2.0f, lightExposure);
		pNewMaterial->setLightIntensty(fullIntensity);

		int lightQuadFalloff = ah.getIntParam("light_quadratic_falloff", 1);
		pNewMaterial->setQuadraticFalloff(lightQuadFalloff == 1);

		int weightBySurfaceArea = ah.getIntParam("light_weight_by_surface_area", 1);
		pNewMaterial->setWeightBySurfaceArea(weightBySurfaceArea == 1);
	}

	return pNewMaterial;
}

