#ifndef KATANA_HELPERS_H
#define KATANA_HELPERS_H

#include <vector>

#include <FnAttribute/FnAttribute.h>

#include <FnScenegraphIterator/FnScenegraphIterator.h>

#ifdef KAT_V_2
#include <FnRenderOutputUtils/XFormMatrix.h>
#else
#include <RenderOutputUtils/XFormMatrix.h>
#endif


#include "colour/colour3f.h"

class KatanaHelpers
{
public:
	KatanaHelpers();

	static FnKat::GroupAttribute buildLocationXformList(FnKat::FnScenegraphIterator iterator, int depthLimit);

	static Foundry::Katana::RenderOutputUtils::XFormMatrixVector getXFormMatrixStatic(FnKat::GroupAttribute xformAttr);
	static Foundry::Katana::RenderOutputUtils::XFormMatrixVector getXFormMatrixStatic(FnKat::FnScenegraphIterator iterator);
	static Foundry::Katana::RenderOutputUtils::XFormMatrixVector getXFormMatrixMB(FnKat::FnScenegraphIterator iterator,
																				  bool clampWithinShutter, float shutterOpen, float shutterClose);

};

// helpers to easily get attributes with fallback defaults
class KatanaAttributeHelper
{
public:
	KatanaAttributeHelper(const FnKat::GroupAttribute& attribute);


	// use member attribute
	float getFloatParam(const std::string& name, float defaultValue) const;
	int getIntParam(const std::string& name, int defaultValue) const;
	Colour3f getColourParam(const std::string& name, const Colour3f& defaultValue) const;
	std::string getStringParam(const std::string& name) const;


	// static versions that can be used stand-alone
	static float getFloatParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, float defaultValue);
	static int getIntParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, int defaultValue);
	static Colour3f getColourParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, const Colour3f& defaultValue);
	static std::string getStringParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name);

protected:
	const FnKat::GroupAttribute&	m_attribute;
};

#endif // KATANA_HELPERS_H
