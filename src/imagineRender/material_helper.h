#ifndef MATERIAL_HELPER_H
#define MATERIAL_HELPER_H

#include <map>
#include <vector>
#include <string>

#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnDataBuilder.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#include <FnRenderOutputUtils/FnRenderOutputUtils.h>

#include "colour/colour3f.h"
#include "core/hash.h"

namespace Imagine
{
	class Material;
	class Texture;
	class Logger;
}

class MaterialHelper
{
public:
	MaterialHelper(Imagine::Logger& logger);

	Imagine::Material* getOrCreateMaterialForLocation(FnKat::FnScenegraphIterator iterator, const FnKat::GroupAttribute& imagineStatements, bool fallbackToDefault = true);

	FnKat::GroupAttribute getMaterialForLocation(FnKat::FnScenegraphIterator iterator) const;

	std::vector<Imagine::Material*>& getMaterialsVector() { return m_aMaterials; }

	Imagine::Material* getDefaultMaterial() { return m_pDefaultMaterial; }

protected:

	// this adds to the instances map itself if materials are created
	Imagine::Material* createNewMaterial(const FnKat::GroupAttribute& attribute, bool isMatte, bool fallbackToDefault);

public:
	// hack for live rendering prototyping
	static Imagine::Material* createNewMaterialStandAlone(const std::string& materialType, const FnKat::GroupAttribute& shaderParamsAttr);

protected:
	// called by createNewMaterial to try and create a network material from the attribute...
	// TODO: all this stuff is pretty horrendous, but while verbose, it's easiest for the moment until we work
	//       out what we're doing with built-in shaders which use static const init registration (so don't work in .so files),
	//       and how to pass in Args in a nice generic way
	Imagine::Material* createNetworkMaterial(const FnKat::GroupAttribute& attribute, bool isMatte);

	static Imagine::Texture* createNetworkOpItem(const std::string& opName, const FnKat::GroupAttribute& params);
	static Imagine::Texture* createNetworkTextureItem(const std::string& textureName, const FnKat::GroupAttribute& params);

	static void connectTextureToMaterial(Imagine::Material* pMaterial, const std::string& shaderName, const std::string& paramName, const Imagine::Texture* pTexture);
	static void connectOpToOp(Imagine::Texture* pTargetOp, const std::string& opName, const std::string& paramName, const Imagine::Texture* pSourceOp);

	// stuff for uber-shaders
	static Imagine::Material* createMaterial(const std::string& materialType, const FnKat::GroupAttribute& shaderParamsAttr);
	static Imagine::Material* createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr,
											FnKat::GroupAttribute& alphaParamsAttr);
	static Imagine::Material* createGlassMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Imagine::Material* createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Imagine::Material* createPlasticMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Imagine::Material* createBrushedMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Imagine::Material* createMetallicPaintMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Imagine::Material* createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Imagine::Material* createVelvetMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Imagine::Material* createLuminousMaterial(const FnKat::GroupAttribute& shaderParamsAttr);

	// stuff for textures
	static Imagine::Texture* createConstantTexture(const FnKat::GroupAttribute& textureParamsAttr);
	static Imagine::Texture* createCheckerboardTexture(const FnKat::GroupAttribute& textureParamsAttr);
	static Imagine::Texture* createGridTexture(const FnKat::GroupAttribute& textureParamsAttr);
	static Imagine::Texture* createSwatchTexture(const FnKat::GroupAttribute& textureParamsAttr);
	static Imagine::Texture* createWireframeTexture(const FnKat::GroupAttribute& textureParamsAttr);
	
	static bool isRecognisedShaderType(const std::string& name);

protected:
	Imagine::Logger&						m_logger;
	FnKat::StringAttribute					m_terminatorNodes;

	std::map<Imagine::HashValue, Imagine::Material*>	m_aMaterialInstances; // all materials with hashes

	std::vector<Imagine::Material*>			m_aMaterials; // all material instances in std::vector

	Imagine::Material*						m_pDefaultMaterial;
	Imagine::Material*						m_pDefaultMaterialMatte; // annoying, but...
};

#endif // MATERIAL_HELPER_H
