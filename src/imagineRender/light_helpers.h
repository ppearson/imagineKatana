#ifndef LIGHT_HELPERS_H
#define LIGHT_HELPERS_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

class Light;

class LightHelpers
{
public:
	LightHelpers();

	Light* createLight(const FnKat::GroupAttribute& lightMaterialAttr);

	Light* createPointLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createSpotLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createAreaLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createDistantLight(const FnKat::GroupAttribute& shaderParamsAttr);

	Light* createSkydomeLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createEnvironmentLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createPhysicalSkyLight(const FnKat::GroupAttribute& shaderParamsAttr);

	static int getFalloffEnumValFromString(const std::string& falloff);
	static int getShadowTypeEnumValFromString(const std::string& shadowType);
};

#endif // LIGHT_HELPERS_H
