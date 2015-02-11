#include "light_helpers.h"

#include "lights/point_light.h"
#include "lights/area_light.h"
#include "lights/spot_light.h"
#include "lights/sky_dome.h"
#include "lights/physical_sky.h"
#include "lights/environment_light.h"
#include "lights/distant_light.h"

#include "katana_helpers.h"

LightHelpers::LightHelpers()
{
}

Light* LightHelpers::createLight(const FnKat::GroupAttribute& lightMaterialAttr)
{
	// make sure we've got something useful...
	FnKat::StringAttribute shaderNameAttr = lightMaterialAttr.getChildByName("imagineLightShader");

	if (!shaderNameAttr.isValid())
		return NULL;

	// get params...
	FnKat::GroupAttribute shaderParamsAttr = lightMaterialAttr.getChildByName("imagineLightParams");
	// if we don't have them, the material must have default settings, so just create the default material
	// of that type

	Light* pNewLight = NULL;

	std::string shaderName = shaderNameAttr.getValue("", false);

	if (shaderName == "Point")
	{
		pNewLight = createPointLight(shaderParamsAttr);
	}
	else if (shaderName == "Spot")
	{
		pNewLight = createSpotLight(shaderParamsAttr);
	}
	else if (shaderName == "Area")
	{
		pNewLight = createAreaLight(shaderParamsAttr);
	}
	else if (shaderName == "Distant")
	{
		pNewLight = createDistantLight(shaderParamsAttr);
	}
	else if (shaderName == "SkyDome")
	{
		pNewLight = createSkydomeLight(shaderParamsAttr);
	}
	else if (shaderName == "Environment")
	{
		pNewLight = createEnvironmentLight(shaderParamsAttr);
	}
	else if (shaderName == "PhysicalSky")
	{
		pNewLight = createPhysicalSkyLight(shaderParamsAttr);
	}

	return pNewLight;
}

Light* LightHelpers::createPointLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	PointLight* pNewLight = new PointLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	int shadowType = ah.getIntParam("shadow_type", 0);
	int falloffType = ah.getIntParam("falloff", 0);

	pNewLight->setIntensity(intensity);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setFalloffType((Light::FalloffType)falloffType);

	return pNewLight;
}

Light* LightHelpers::createSpotLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	SpotLight* pNewLight = new SpotLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	int shadowType = ah.getIntParam("shadow_type", 0);
	int numSamples = ah.getIntParam("num_samples", 1);
	int falloffType = ah.getIntParam("falloff", 0);

	float coneAngle = ah.getFloatParam("cone_angle", 30.0f);
	float penumbraAngle = ah.getFloatParam("penumbra_angle", 5.0f);
	bool isArea = ah.getIntParam("is_area", 1) == 1;

	pNewLight->setIntensity(intensity);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setFalloffType((Light::FalloffType)falloffType);
	pNewLight->setSamples(numSamples);

	pNewLight->setConeAngle(coneAngle);
	pNewLight->setPenumbraAngle(penumbraAngle);
	pNewLight->setIsArea(isArea);

	return pNewLight;
}

Light* LightHelpers::createAreaLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	AreaLight* pNewLight = new AreaLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	int shadowType = ah.getIntParam("shadow_type", 0);
	int falloffType = ah.getIntParam("falloff", 2);
	int numSamples = ah.getIntParam("num_samples", 1);

	float width = ah.getFloatParam("width", 1.0f);
	float depth = ah.getFloatParam("depth", 1.0f);

	int shapeType = ah.getFloatParam("shape_type", 0);

	bool isScale = ah.getIntParam("scale", 1) == 1;
	bool isVisible = ah.getIntParam("visible", 1) == 1;

	pNewLight->setIntensity(intensity);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setFalloffType((Light::FalloffType)falloffType);
	pNewLight->setSamples(numSamples);
	pNewLight->setVisible(isVisible);
	pNewLight->setDimensions(width, depth);
	pNewLight->setScale(isScale);

	pNewLight->setShapeType((AreaLight::ShapeType)shapeType);

	return pNewLight;
}

Light* LightHelpers::createDistantLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	DistantLight* pNewLight = new DistantLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	int shadowType = ah.getIntParam("shadow_type", 0);
	int falloffType = ah.getIntParam("falloff", 0);
	int numSamples = ah.getIntParam("num_samples", 1);

	float angle = ah.getFloatParam("spread_angle", 1.0f);

	pNewLight->setIntensity(intensity);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setFalloffType((Light::FalloffType)falloffType);
	pNewLight->setSamples(numSamples);
//	pNewLight->setVisible(isVisible);
	pNewLight->setSpreadAngle(angle);

	return pNewLight;
}

Light* LightHelpers::createSkydomeLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	SkyDome* pNewLight = new SkyDome();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	int shadowType = ah.getIntParam("shadow_type", 0);
	int numSamples = ah.getIntParam("num_samples", 1);
	bool isVisible = ah.getIntParam("visible", 1) == 1;

	pNewLight->setIntensity(intensity);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setSamples(numSamples);
	pNewLight->setRadius(2000.0f);
	pNewLight->setVisible(isVisible);

	return pNewLight;
}

Light* LightHelpers::createEnvironmentLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	EnvironmentLight* pNewLight = new EnvironmentLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	int shadowType = ah.getIntParam("shadow_type", 0);
	int numSamples = ah.getIntParam("num_samples", 1);
	int isVisible = ah.getIntParam("visible", 1);

	std::string envMapPath = ah.getStringParam("env_map_path");

	pNewLight->setIntensity(intensity);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setSamples(numSamples);
	pNewLight->setEnvMapPath(envMapPath);
	pNewLight->setRadius(2000.0f);
	pNewLight->setVisible(isVisible == 1);

	return pNewLight;
}

Light* LightHelpers::createPhysicalSkyLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	PhysicalSky* pNewLight = new PhysicalSky();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	int shadowType = ah.getIntParam("shadow_type", 0);
	int numSamples = ah.getIntParam("num_samples", 1);
	int isVisible = ah.getIntParam("visible", 1);

	int dayOfYear = ah.getIntParam("day", 174);
	float time = ah.getFloatParam("time", 17.1f);

	float skyIntensity = ah.getFloatParam("sky_intensity", 1.0f);
	float sunIntensity = ah.getFloatParam("sun_intensity", 1.0f);

	float turbidity = ah.getFloatParam("turbidity", 3.0f);

	int hemisphereExtend = ah.getIntParam("hemisphere_extension", 0);

	pNewLight->setIntensity(intensity);
	pNewLight->setShadowType((Light::ShadowType)shadowType);
	pNewLight->setSamples(numSamples);
	pNewLight->setRadius(2000.0f);
	pNewLight->setVisible(isVisible == 1);

	pNewLight->setTurbidity(turbidity);

	pNewLight->setDayOfYear(dayOfYear);
	pNewLight->setTimeOfDay(time);

	pNewLight->setIntensityScales(skyIntensity, sunIntensity);

	pNewLight->setHemiExtend((unsigned char)hemisphereExtend);

	return pNewLight;
}
