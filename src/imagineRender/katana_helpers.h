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

#ifndef KATANA_HELPERS_H
#define KATANA_HELPERS_H

#include <vector>

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>
#include <FnRenderOutputUtils/XFormMatrix.h>


#include "colour/colour3f.h"

class KatanaHelpers
{
public:
	KatanaHelpers();

	static FnKat::GroupAttribute buildLocationXformList(const FnKat::FnScenegraphIterator& iterator, int depthLimit);

	static Foundry::Katana::RenderOutputUtils::XFormMatrixVector getXFormMatrixStatic(const FnKat::GroupAttribute& xformAttr);
	static Foundry::Katana::RenderOutputUtils::XFormMatrixVector getXFormMatrixStatic(const FnKat::FnScenegraphIterator& iterator);
	static Foundry::Katana::RenderOutputUtils::XFormMatrixVector getXFormMatrixMB(const FnKat::FnScenegraphIterator& iterator,
																				  bool clampWithinShutter, float shutterOpen, float shutterClose);

	static void getRelevantSampleTimes(const FnKat::DataAttribute& attribute, std::vector<float>& aSampleTimes, float shutterOpen, float shutterClose);

};

// helpers to easily get attributes with fallback defaults
class KatanaAttributeHelper
{
public:
	KatanaAttributeHelper(const FnKat::GroupAttribute& attribute);


	// use member attribute
	float getFloatParam(const std::string& name, float defaultValue) const;
	int getIntParam(const std::string& name, int defaultValue) const;
	Imagine::Colour3f getColourParam(const std::string& name, const Imagine::Colour3f& defaultValue) const;
	std::string getStringParam(const std::string& name, const std::string& defaultValue = "") const;


	// static versions that can be used stand-alone
	static float getFloatParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, float defaultValue);
	static int getIntParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, int defaultValue);
	static Imagine::Colour3f getColourParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, const Imagine::Colour3f& defaultValue);
	static std::string getStringParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, const std::string& defaultValue);

protected:
	const FnKat::GroupAttribute&	m_attribute;
};

#endif // KATANA_HELPERS_H
