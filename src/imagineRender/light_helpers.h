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
	static int getAreaLightShapeTypeEnumValFromString(const std::string& shapeType);
};

#endif // LIGHT_HELPERS_H
