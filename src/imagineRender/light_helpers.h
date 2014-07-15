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

	Light* createSkydomeLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createEnvironmentLight(const FnKat::GroupAttribute& shaderParamsAttr);
	Light* createPhysicalSkyLight(const FnKat::GroupAttribute& shaderParamsAttr);
};

#endif // LIGHT_HELPERS_H
