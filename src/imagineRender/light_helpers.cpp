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

#include "light_helpers.h"

#include "lights/point_light.h"
#include "lights/area_light.h"
#include "lights/spot_light.h"
#include "lights/sky_dome.h"
#include "lights/physical_sky.h"
#include "lights/environment_light.h"
#include "lights/distant_light.h"

#include "katana_helpers.h"

using namespace Imagine;

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
	float exposure = ah.getFloatParam("exposure", 0.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	std::string falloff = ah.getStringParam("falloff", "quadratic");
	int falloffType = getFalloffEnumValFromString(falloff);

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
	pNewLight->setFalloffType((Light::FalloffType)falloffType);

	return pNewLight;
}

Light* LightHelpers::createSpotLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	SpotLight* pNewLight = new SpotLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	float exposure = ah.getFloatParam("exposure", 0.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	int numSamples = ah.getIntParam("num_samples", 1);
	std::string falloff = ah.getStringParam("falloff", "quadratic");
	int falloffType = getFalloffEnumValFromString(falloff);

	float coneAngle = ah.getFloatParam("cone_angle", 30.0f);
	float penumbraAngle = ah.getFloatParam("penumbra_angle", 5.0f);
	bool isArea = ah.getIntParam("is_area", 1) == 1;

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
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
	float exposure = ah.getFloatParam("exposure", 0.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	std::string falloff = ah.getStringParam("falloff", "quadratic");
	int falloffType = getFalloffEnumValFromString(falloff);
	int numSamples = ah.getIntParam("num_samples", 1);

	float width = ah.getFloatParam("width", 1.0f);
	float depth = ah.getFloatParam("depth", 1.0f);

	int shapeType = ah.getFloatParam("shape_type", 0);

	bool isScale = ah.getIntParam("scale", 1) == 1;

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
	pNewLight->setFalloffType((Light::FalloffType)falloffType);
	pNewLight->setSamples(numSamples);
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
	float exposure = ah.getFloatParam("exposure", 0.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	int numSamples = ah.getIntParam("num_samples", 1);
	float angle = ah.getFloatParam("spread_angle", 1.0f);

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
	pNewLight->setSamples(numSamples);
	pNewLight->setSpreadAngle(angle);

	return pNewLight;
}

Light* LightHelpers::createSkydomeLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	SkyDome* pNewLight = new SkyDome();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	float exposure = ah.getFloatParam("exposure", 0.0f);
	Colour3f colour = ah.getColourParam("colour", Colour3f(1.0f));
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	int numSamples = ah.getIntParam("num_samples", 1);

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setColour(colour);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
	pNewLight->setSamples(numSamples);
	pNewLight->setRadius(2000.0f);

	return pNewLight;
}

Light* LightHelpers::createEnvironmentLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	EnvironmentLight* pNewLight = new EnvironmentLight();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	float exposure = ah.getFloatParam("exposure", 0.0f);
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	int numSamples = ah.getIntParam("num_samples", 1);
	bool clampLuminance = ah.getIntParam("clamp_luminance", 0) == 1;

	std::string envMapPath = ah.getStringParam("env_map_path");

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
	pNewLight->setSamples(numSamples);
	pNewLight->setEnvMapPath(envMapPath);
	pNewLight->setRadius(2000.0f);
	pNewLight->setClampLuminance(clampLuminance);

	return pNewLight;
}

Light* LightHelpers::createPhysicalSkyLight(const FnKat::GroupAttribute& shaderParamsAttr)
{
	PhysicalSky* pNewLight = new PhysicalSky();

	KatanaAttributeHelper ah(shaderParamsAttr);

	float intensity = ah.getFloatParam("intensity", 1.0f);
	float exposure = ah.getFloatParam("exposure", 0.0f);
	std::string shadowType = ah.getStringParam("shadow_type", "normal");
	int shadowTypeEnum = getShadowTypeEnumValFromString(shadowType);
	int numSamples = ah.getIntParam("num_samples", 1);
	bool clampLuminance = ah.getIntParam("clamp_luminance", 0) == 1;

	int dayOfYear = ah.getIntParam("day", 174);
	float time = ah.getFloatParam("time", 17.2f);

	float skyIntensity = ah.getFloatParam("sky_intensity", 1.0f);
	float sunIntensity = ah.getFloatParam("sun_intensity", 1.0f);

	float turbidity = ah.getFloatParam("turbidity", 3.0f);

	int hemisphereExtend = ah.getIntParam("hemisphere_extension", 0);

	pNewLight->setIntensity(intensity);
	pNewLight->setExposure(exposure);
	pNewLight->setShadowType((Light::ShadowType)shadowTypeEnum);
	pNewLight->setSamples(numSamples);
	pNewLight->setRadius(2000.0f);
	pNewLight->setClampLuminance(clampLuminance);

	pNewLight->setTurbidity(turbidity);

	pNewLight->setDayOfYear(dayOfYear);
	pNewLight->setTimeOfDay(time);

	pNewLight->setIntensityScales(skyIntensity, sunIntensity);

	pNewLight->setHemiExtend((unsigned char)hemisphereExtend);

	return pNewLight;
}

int LightHelpers::getFalloffEnumValFromString(const std::string& falloff)
{
	if (falloff == "none")
		return 0;
	else if (falloff == "linear")
		return 1;
	else
		return 2;
}

int LightHelpers::getShadowTypeEnumValFromString(const std::string& shadowType)
{
	if (shadowType == "transparent")
		return 1;
	else if (shadowType == "none")
		return 2;
	else
		return 0;
}
