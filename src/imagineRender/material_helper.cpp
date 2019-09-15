/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

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

#include "textures/shader_ops/op_adjust.h"
#include "textures/shader_ops/op_invert.h"
#include "textures/shader_ops/op_mix.h"

#include "textures/constant.h"
#include "textures/shader_ops/op_texture_read.h"
#include "textures/procedural_2d/checkerboard_2d.h"
#include "textures/procedural_2d/gridlines.h"
#include "textures/procedural_2d/swatch.h"
#include "textures/procedural_2d/wireframe.h"

#include "utils/logger.h"

#include "katana_helpers.h"

using namespace Imagine;

MaterialHelper::MaterialHelper(Imagine::Logger& logger) : m_logger(logger), m_pDefaultMaterial(NULL), m_pDefaultMaterialMatte(NULL)
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

Material* MaterialHelper::getOrCreateMaterialForLocation(const FnKat::FnScenegraphIterator& iterator, const FnKat::GroupAttribute& imagineStatements, bool fallbackToDefault)
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

	FnAttribute::Hash materialRawHash = materialAttrib.getHash();
	Hash hash;
	hash.addLongLong(materialRawHash.uint64());
	hash.addUChar((unsigned char)isMatte);
	HashValue materialHash = hash.getHash();

	std::map<HashValue, Material*>::const_iterator itFind = m_aMaterialInstances.find(materialHash);

	if (itFind != m_aMaterialInstances.end())
	{
		pMaterial = (*itFind).second;

		if (pMaterial)
			return pMaterial;
	}

	// otherwise, create a new material
	pMaterial = createNewMaterial(materialAttrib, isMatte, fallbackToDefault);

	if (pMaterial)
	{
		// add this new material to our list of material instances
		m_aMaterialInstances[materialHash] = pMaterial;
		m_aMaterials.push_back(pMaterial);
	}

	return pMaterial;
}

FnKat::GroupAttribute MaterialHelper::getMaterialForLocation(const FnKat::FnScenegraphIterator& iterator) const
{
	// Note: this only gets the exact material attributes we asked for if it's not a Network Material - if
	//       it is a Network Material, we get everything bundled together, so it's possible nodes and shaders
	//       for multiple renderers are bundled together in different GroupAttribute batches for each renderer
	return FnKat::RenderOutputUtils::getFlattenedMaterialAttr(iterator, m_terminatorNodes);
}

Material* MaterialHelper::createNewMaterial(const FnKat::GroupAttribute& attribute, bool isMatte, bool fallbackToDefault)
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
		if (!pNewMaterial && fallbackToDefault)
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

	if (fallbackToDefault)
	{
		return m_pDefaultMaterial;
	}
	else
	{
		return NULL;
	}
}

Material* MaterialHelper::createNewMaterialStandAlone(const std::string& materialType, const FnKat::GroupAttribute& shaderParamsAttr)
{
	return createMaterial(materialType, shaderParamsAttr);
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

	// make sure we at least have an Imagine-specific shader node within the terminals list before continuing processing
	FnKat::StringAttribute imagineSurfaceAttr = terminalsAttr.getChildByName("imagineSurface");
	FnKat::StringAttribute imagineBumpAttr = terminalsAttr.getChildByName("imagineBump");
	FnKat::StringAttribute imagineAlphaAttr = terminalsAttr.getChildByName("imagineAlpha");

	bool haveValidImagineTerminalType = imagineSurfaceAttr.isValid() || imagineBumpAttr.isValid() || imagineAlphaAttr.isValid();
	if (!haveValidImagineTerminalType)
	{
		// don't bother continuing processing the material, as there's nothing we want
//		fprintf(stderr, "\n\n%s\n\n\n", attribute.getXML().c_str());
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
		bool isNonNetworkShaderType = isRecognisedShaderType(nodeType);

		if (!isShader && !isOp && !isTexture && !isNonNetworkShaderType)
		{
			// if we don't know what it is, it's probably not for us, so just skip it
			m_logger.warning("Unknown network material type: %s", nodeType.c_str());
			continue;
		}

		FnKat::GroupAttribute paramsAttr = subItem.getChildByName("parameters");

		if (isNonNetworkShaderType)
		{
			// it wasn't actually a network shader, Katana just puts it in that group because that location inherited
			// network materials from higher up
			pNewNodeMaterial = createMaterial(nodeType, paramsAttr);

			if (!pNewNodeMaterial)
				continue;

			aMaterialNodes[nodeName] = pNewNodeMaterial;
		}
		else if (isShader)
		{
			std::string shaderType = nodeType.substr(7);

			pNewNodeMaterial = createMaterial(shaderType, paramsAttr);

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

				if (connItemAttr.isValid() && connItemAttr.getValue("", false).find('@') != std::string::npos)
				{
					std::string connectedItem = connItemAttr.getValue("", false);

					size_t atPos = connectedItem.find('@');

					std::string portName = connectedItem.substr(0, atPos);
					std::string connectionNodeName = connectedItem.substr(atPos + 1);

					bool foundConnection = false;

					// now find what it's connected to

					// first see if it's an op
					std::map<std::string, Texture*>::const_iterator itFindItem = aOpNodes.find(connectionNodeName);
					if (itFindItem != aOpNodes.end())
					{
						const Texture* pConn = itFindItem->second;
						connectTextureToMaterial(pMaterial, shaderName, paramName, pConn);
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
					m_logger.error("Can't find existing Node: %s for connection: %s", connectionNodeName.c_str(), paramName.c_str());
					continue;
				}
				else
				{
					m_logger.error("Invalid connection item...");
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

				if (connItemAttr.isValid() && connItemAttr.getValue("", false).find('@') != std::string::npos)
				{
					std::string connectedItem = connItemAttr.getValue("", false);

					size_t atPos = connectedItem.find('@');

					std::string portName = connectedItem.substr(0, atPos);
					std::string connectionNodeName = connectedItem.substr(atPos + 1);

					// now find what it's connected to
					// for the moment, hopefully we're only going to be connecting Ops (Textures)

					std::map<std::string, Texture*>::const_iterator itFindItem = aOpNodes.find(connectionNodeName);
					// otherwise, see if it was a texture
					if (itFindItem == aOpNodes.end())
					{
						itFindItem = aTextureNodes.find(connectionNodeName);
					}

					if (itFindItem == aOpNodes.end() || itFindItem == aTextureNodes.end())
					{
						m_logger.error("Can't find existing Node: %s for connection: %s", connectionNodeName.c_str(), paramName.c_str());
						continue;
					}

					const Texture* pConn = itFindItem->second;

					connectOpToOp(pTargetTexture, opName, paramName, pConn);
				}
				else
				{
					m_logger.error("Invalid connection item...");
				}
			}
		}
	}

	// now go through terminals connecting them up...
	// for the moment, we can just do surface ones...

	return pNewNodeMaterial;
}

// TODO: these are increadibly hacky - this sort of infrastructure should be moved into Imagine properly and be
//       passed in as generic Args attributes

Texture* MaterialHelper::createNetworkOpItem(const std::string& opName, const FnKat::GroupAttribute& params)
{
	// TODO: do something better than this...
	
	Texture* pNewOp = NULL;
	
	KatanaAttributeHelper ah(params);

	if (opName == "Adjust")
	{
		OpAdjust* pTypedNewOp = new OpAdjust();
		
		pTypedNewOp->setAdjustValue(ah.getFloatParam("adjust_value", 1.0f));
		
		return pTypedNewOp;
	}
	else if (opName == "Mix")
	{
		OpMix* pTypedNewOp = new OpMix();
		
		pTypedNewOp->setConstantMixValue(ah.getFloatParam("mix_value", 0.5f));
		
		return pTypedNewOp;
	}
	
	return pNewOp;
}

Texture* MaterialHelper::createNetworkTextureItem(const std::string& textureName, const FnKat::GroupAttribute& params)
{
	// TODO: do something better than this...

	Texture* pNewTexture = NULL;

	if (textureName == "TextureRead")
	{
		pNewTexture = createTextureReadTexture(params);
	}
	else if (textureName == "Constant")
	{
		pNewTexture = createConstantTexture(params);
	}
	else if (textureName == "Checkerboard")
	{
		pNewTexture = createCheckerboardTexture(params);
	}
	else if (textureName == "Grid")
	{
		pNewTexture = createGridTexture(params);
	}
	else if (textureName == "Swatch")
	{
		pNewTexture = createSwatchTexture(params);
	}
	else if (textureName == "Wireframe")
	{
		pNewTexture = createWireframeTexture(params);
	}
	return pNewTexture;
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
			pSM->setSpecularColourManualTexture(pTexture);
		}
		else if (paramName == "spec_roughness")
		{
			pSM->setSpecularRoughnessManualTexture(pTexture);
		}
		else if (paramName == "diffuse_roughness_texture")
		{

		}
		else if (paramName == "diff_backlit_texture")
		{

		}
	}
}

// TODO: again, just making this worse and worse instead of doing things properly with the Args API, but...
void MaterialHelper::connectOpToOp(Texture* pTargetOp, const std::string& opName, const std::string& paramName, const Texture* pSourceOp)
{
	if (opName == "Adjust")
	{
		OpAdjust* pOA = static_cast<OpAdjust*>(pTargetOp);
		
		if (paramName == "input")
		{
			pOA->setInputTexture(pSourceOp);
		}
	}
	else if (opName == "Mix")
	{
		OpMix* pOM = static_cast<OpMix*>(pTargetOp);
		
		if (paramName == "mix_amount")
		{
			pOM->setInputTexture(pSourceOp);
		}
		else if (paramName == "input_A")
		{
			pOM->setInputATexture(pSourceOp);
		}
		else if (paramName == "input_B")
		{
			pOM->setInputBTexture(pSourceOp);
		}
	}
}

// TODO: this is crap and is just getting silly - need to finish the Args interface API so none of this hard-coded stuff
//       is needed
Material* MaterialHelper::createMaterial(const std::string& materialType, const FnKat::GroupAttribute& shaderParamsAttr)
{
	// TODO: duplicate of above code, but...
	FnKat::GroupAttribute bumpParamsAttr;
	FnKat::GroupAttribute alphaParamsAttr;

	Material* pNewMaterial = NULL;

	if (materialType == "Standard" || materialType == "StandardImage")
	{
		pNewMaterial = createStandardMaterial(shaderParamsAttr, bumpParamsAttr, alphaParamsAttr);
	}
	else if (materialType == "Glass")
	{
		pNewMaterial = createGlassMaterial(shaderParamsAttr);
	}
	else if (materialType == "Metal")
	{
		pNewMaterial = createMetalMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (materialType == "Plastic")
	{
		pNewMaterial = createPlasticMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (materialType == "Metallic Paint")
	{
		pNewMaterial = createMetallicPaintMaterial(shaderParamsAttr, bumpParamsAttr);
	}
	else if (materialType == "Translucent")
	{
		pNewMaterial = createTranslucentMaterial(shaderParamsAttr, bumpParamsAttr);
	}

	return pNewMaterial;
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
	}

	float specRoughness = ah.getFloatParam("spec_roughness", 0.15f);
	pNewStandardMaterial->setSpecularRoughness(specRoughness);

	std::string specRoughnessTexture = ah.getStringParam("spec_roughness_texture");
	if (!specRoughnessTexture.empty())
	{
		pNewStandardMaterial->setSpecularRoughnessTextureMapPath(specRoughnessTexture, true);
	}

	std::string microfacetType = ah.getStringParam("microfacet_type", "beckmann");
	int specType = 2;
	if (microfacetType == "phong")
		specType = 0;
	else if (microfacetType == "beckmann")
		specType = 2;
	else if (microfacetType == "ggx")
		specType = 3;

	pNewStandardMaterial->setSpecularType(specType);

	float reflection = ah.getFloatParam("reflection", 0.0f);
	pNewStandardMaterial->setReflection(reflection);

	float reflectionRoughness = ah.getFloatParam("reflection_roughness", 0.0f);
	pNewStandardMaterial->setReflectionRoughness(reflectionRoughness);

	float refractionIndex = ah.getFloatParam("refraction_index", 1.49f);

	int fresnelEnabled = ah.getIntParam("fresnel", 1);
	if (fresnelEnabled == 1)
	{
		pNewStandardMaterial->setFresnelEnabled(true);

		float fresnelCoefficient = ah.getFloatParam("fresnel_coef", 0.0f);
		pNewStandardMaterial->setFresnelCoefficient(fresnelCoefficient);

		if (refractionIndex == 1.0f)
			refractionIndex = 1.4f;
	}
	else
	{
		pNewStandardMaterial->setFresnelEnabled(false);
	}

	pNewStandardMaterial->setRefractionIndex(refractionIndex);

	float transparency = ah.getFloatParam("transparency", 0.0f);
	pNewStandardMaterial->setTransparancy(transparency);
	
	float transmittance = ah.getFloatParam("transmittance", 1.0f);
	pNewStandardMaterial->setTransmittance(transmittance);

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

	std::string microfacetType = ah.getStringParam("microfacet_type", "beckmann");
	int microfacetModel = 1;
	if (microfacetType == "phong")
		microfacetModel = 0;
	else if (microfacetType == "beckmann")
		microfacetModel = 1;
	else if (microfacetType == "ggx")
		microfacetModel = 2;

	pNewMaterial->setMicrofacetModel(microfacetModel);

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

	Colour3f surfaceColour = ah.getColourParam("surface_col", Colour3f(0.0f, 0.0f, 0.0f));
	pNewMaterial->setSurfaceColour(surfaceColour);

	std::string surfaceColourTexture = ah.getStringParam("surface_col_texture");
	if (!surfaceColourTexture.empty())
	{
		pNewMaterial->setSurfaceColourTextureMapPath(surfaceColourTexture, true); // lazy load texture when needed
	}

	Colour3f specularColour = ah.getColourParam("specular_col", Colour3f(0.1f, 0.1f, 0.1f));
	pNewMaterial->setSpecularColour(specularColour);

	std::string specularColourTexture = ah.getStringParam("specular_col_texture");
	if (!specularColourTexture.empty())
	{
		pNewMaterial->setSpecularColourTextureMapPath(specularColourTexture, true); // lazy load texture when needed
	}

	float surfaceRoughness = ah.getFloatParam("specular_roughness", 0.05f);
	pNewMaterial->setSpecularRoughness(surfaceRoughness);

	std::string surfaceType = ah.getStringParam("surface_type", "diffuse");
	if (surfaceType == "diffuse")
	{
		pNewMaterial->setSurfaceType(0);
	}
	else if (surfaceType == "dielectric layer")
	{
		pNewMaterial->setSurfaceType(1);
	}
	else if (surfaceType == "transmission only")
	{
		pNewMaterial->setSurfaceType(2);
	}

	std::string scatterMode = ah.getStringParam("scatter_mode", "mean free path");
	if (scatterMode == "legacy")
	{
		pNewMaterial->setScatteringMode(0);

		Colour3f innerColour = ah.getColourParam("inner_col", Colour3f(0.4f, 0.4f, 0.4f));
		pNewMaterial->setInnerColour(innerColour);

		float subsurfaceDensity = ah.getFloatParam("subsurface_density", 3.1f);
		pNewMaterial->setSubsurfaceDensity(subsurfaceDensity);
	}
	else
	{
		pNewMaterial->setScatteringMode(1);

		Colour3f mfp = ah.getColourParam("mfp", Colour3f(0.22f, 0.081f, 0.06f));
		pNewMaterial->setMeanFreePath(mfp);

		float mfpScale = ah.getFloatParam("mfp_scale", 2.7f);
		pNewMaterial->setMeanFreePathScale(mfpScale);

		Colour3f scatterAlbedo = ah.getColourParam("scatter_albedo", Colour3f(0.3f));
		pNewMaterial->setScatterAlbedo(scatterAlbedo);
	}

	float samplingSensity = ah.getFloatParam("sampling_density", 0.65f);
	pNewMaterial->setSamplingDensity(samplingSensity);

	unsigned int scatterLimit = ah.getIntParam("scatter_limit", 6);
	pNewMaterial->setScatterLimit(scatterLimit);

	float transmittance = ah.getFloatParam("transmittance", 1.0f);
	pNewMaterial->setTransmittance(transmittance);
	float transmittanceRoughness = ah.getFloatParam("transmittance_roughness", 0.8f);
	pNewMaterial->setTransmittanceRoughness(transmittanceRoughness);

	bool surfaceColourAsTransmittance = ah.getIntParam("use_surf_col_as_trans", 0) == 1;
	pNewMaterial->setUseSurfaceColourAsTransmittance(surfaceColourAsTransmittance);

	std::string entryExitType = ah.getStringParam("entry_exit_type");
	if (entryExitType == "refractive fresnel")
	{
		pNewMaterial->setEntryExitType(0);
	}
	else if (entryExitType == "refractive")
	{
		pNewMaterial->setEntryExitType(1);
	}
	else if (entryExitType == "transmissive")
	{
		pNewMaterial->setEntryExitType(2);
	}

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

//

Texture* MaterialHelper::createConstantTexture(const FnKat::GroupAttribute& textureParamsAttr)
{
	Constant* pNewTexture = new Constant();

	KatanaAttributeHelper ah(textureParamsAttr);

	Colour3f colour = ah.getColourParam("colour", Colour3f(0.6f, 0.6f, 0.6f));
	pNewTexture->setColour(colour);

	return pNewTexture;
}

Texture* MaterialHelper::createCheckerboardTexture(const FnKat::GroupAttribute& textureParamsAttr)
{
	Checkerboard2D* pNewTexture = new Checkerboard2D();

	KatanaAttributeHelper ah(textureParamsAttr);

	Colour3f colour1 = ah.getColourParam("colour1", Colour3f(0.0f, 0.0f, 0.0f));
	pNewTexture->setColour1(colour1);
	Colour3f colour2 = ah.getColourParam("colour2", Colour3f(1.0f, 1.0f, 1.0f));
	pNewTexture->setColour2(colour2);

	float scaleU = ah.getFloatParam("scaleU", 1.0f);
	float scaleV = ah.getFloatParam("scaleV", 1.0f);

	pNewTexture->setScaleValues(scaleU, scaleV);

	return pNewTexture;
}

Texture* MaterialHelper::createGridTexture(const FnKat::GroupAttribute& textureParamsAttr)
{
	Gridlines* pNewTexture = new Gridlines();

	KatanaAttributeHelper ah(textureParamsAttr);

	Colour3f colour1 = ah.getColourParam("colour1", Colour3f(0.0f, 0.0f, 0.0f));
	pNewTexture->setColour1(colour1);
	Colour3f colour2 = ah.getColourParam("colour2", Colour3f(1.0f, 1.0f, 1.0f));
	pNewTexture->setColour2(colour2);

	float scaleU = ah.getFloatParam("scaleU", 1.0f);
	float scaleV = ah.getFloatParam("scaleV", 1.0f);

	pNewTexture->setScaleValues(scaleU, scaleV);

	return pNewTexture;
}

Texture* MaterialHelper::createSwatchTexture(const FnKat::GroupAttribute& textureParamsAttr)
{
	Swatch* pNewTexture = new Swatch();

	KatanaAttributeHelper ah(textureParamsAttr);

	std::string gridType = ah.getStringParam("grid_type", "hue");
	if (gridType == "none")
		pNewTexture->setGridType(0);
	else if (gridType == "fixed colour")
		pNewTexture->setGridType(2);

	bool checkerboard = ah.getIntParam("checkerboard", 0) == 1;
	pNewTexture->setCheckerboard(checkerboard);

	float scaleU = ah.getFloatParam("scaleU", 1.0f);
	float scaleV = ah.getFloatParam("scaleV", 1.0f);

	pNewTexture->setScaleValues(scaleU, scaleV);

	return pNewTexture;
}

Imagine::Texture* MaterialHelper::createTextureReadTexture(const FnKat::GroupAttribute& textureParamsAttr)
{
	OpTextureRead* pNewTexture = new OpTextureRead();
	
	KatanaAttributeHelper ah(textureParamsAttr);
	
	std::string texturePath = ah.getStringParam("texture_path");
	
	pNewTexture->setTexturePath(texturePath);
	
	return pNewTexture;
}

Texture* MaterialHelper::createWireframeTexture(const FnKat::GroupAttribute& textureParamsAttr)
{
	Wireframe* pNewTexture = new Wireframe();

	KatanaAttributeHelper ah(textureParamsAttr);

	int edgeType = ah.getIntParam("edge_type", 1);
	pNewTexture->setEdgeType((unsigned char)edgeType);

	Colour3f interiorColour = ah.getColourParam("interior_colour", Colour3f(0.6f, 0.6f, 0.6f));
	pNewTexture->setInteriorColour(interiorColour);

	float lineWidth = ah.getFloatParam("line_width", 0.005f);
	pNewTexture->setLineWidth(lineWidth);

	Colour3f lineColour = ah.getColourParam("line_colour", Colour3f(0.01f, 0.01f, 0.01f));
	pNewTexture->setLineColour(lineColour);

	float edgeSoftness = ah.getFloatParam("edge_softness", 0.3f);
	pNewTexture->setEdgeSoftness(edgeSoftness);

	return pNewTexture;
}

bool MaterialHelper::isRecognisedShaderType(const std::string& name)
{
	if (name == "Standard" || name == "StandardImage" || name == "Metal" || name == "Plastic" || name == "Glass" || name == "Translucent")
	{
		return true;
	}

	return false;
}

