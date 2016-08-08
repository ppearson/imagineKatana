#ifndef LIGHT_HELPERS_H
#define LIGHT_HELPERS_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

namespace Imagine
{
	class Light;
}

class LightHelpers
{
public:
	LightHelpers();

	Imagine::Light* createLight(const FnKat::GroupAttribute& lightMaterialAttr);

	Imagine::Light* createPointLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Imagine::Light* createSpotLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Imagine::Light* createAreaLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Imagine::Light* createDistantLight(const FnKat::GroupAttribute& shaderParamsAttr);

	Imagine::Light* createSkydomeLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Imagine::Light* createEnvironmentLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Imagine::Light* createPhysicalSkyLight(const FnKat::GroupAttribute& shaderParamsAttr);

	static int getFalloffEnumValFromString(const std::string& falloff);
	static int getShadowTypeEnumValFromString(const std::string& shadowType);
};

#endif // LIGHT_HELPERS_H
