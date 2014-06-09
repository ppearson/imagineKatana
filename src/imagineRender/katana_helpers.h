#ifndef KATANA_HELPERS_H
#define KATANA_HELPERS_H

#include <FnAttribute/FnAttribute.h>

#include "colour/colour3f.h"

class KatanaHelpers
{
public:
	KatanaHelpers();

};

// helpers to get attribs with fallback defaults easily
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
