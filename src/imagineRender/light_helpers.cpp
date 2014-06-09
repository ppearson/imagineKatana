#include "light_helpers.h"

#include "lights/point_light.h"
#include "lights/area_light.h"
#include "lights/sky_dome.h"
#include "lights/physical_sky.h"
#include "lights/environment_light.h"

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

	std::string shaderName = shaderNameAttr.getValue(shaderName, false);

	if (shaderName == "Point")
	{
		pNewLight = createPointLight(shaderParamsAttr);
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

	return pNewLight;
}

Light* LightHelpers::createSkydomeLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	SkyDome* pNewLight = new SkyDome();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	int numSamples = ah.getIntParam("num_samples", 1);

	pNewLight->setIntensity(intensity);
	pNewLight->setColour(colour);
	pNewLight->setSamples(numSamples);

	return pNewLight;
}

Light* LightHelpers::createEnvironmentLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	EnvironmentLight* pNewLight = new EnvironmentLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	int numSamples = ah.getIntParam("num_samples", 1);

	std::string envMapPath = ah.getStringParam("env_map_path");

	pNewLight->setIntensity(intensity);
	pNewLight->setSamples(numSamples);
	pNewLight->setEnvMapPath(envMapPath);
	pNewLight->setRadius(1000.0f);

	return pNewLight;
}

Light* LightHelpers::createPhysicalSkyLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	PhysicalSky* pNewLight = new PhysicalSky();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	int numSamples = ah.getIntParam("num_samples", 1);

	int dayOfYear = ah.getIntParam("day", 174);
	float time = ah.getFloatParam("time", 17.1f);

	float skyIntensity = ah.getFloatParam("sky_intensity", 1.0f);
	float sunIntensity = ah.getFloatParam("sun_intensity", 1.0f);

	pNewLight->setIntensity(intensity);
	pNewLight->setSamples(numSamples);
	pNewLight->setRadius(1000.0f);

	pNewLight->setDayOfYear(dayOfYear);
	pNewLight->setTimeOfDay(time);

	pNewLight->setIntensityScales(skyIntensity, sunIntensity);

	return pNewLight;
}
