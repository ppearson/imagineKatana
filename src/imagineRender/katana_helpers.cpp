#include "katana_helpers.h"

#include <stdio.h>

#include <set>

#include <FnAttribute/FnGroupBuilder.h>

#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#include <FnGeolibServices/FnXFormUtil.h>
#include <FnAttribute/FnAttributeUtils.h>

using namespace Imagine;

KatanaHelpers::KatanaHelpers()
{
}

FnKat::GroupAttribute KatanaHelpers::buildLocationXformList(FnKat::FnScenegraphIterator iterator, int depthLimit)
{
	int limit = depthLimit;

	std::vector<FnKat::GroupAttribute> aXFormAttributes;

	FnKat::FnScenegraphIterator localIt = iterator;

	while (localIt.isValid() && limit > 0)
	{
		FnKat::GroupAttribute localXFormAttribute = localIt.getAttribute("xform");

		if (localXFormAttribute.isValid())
		{
			aXFormAttributes.push_back(localXFormAttribute);
		}

		limit --;

		if (limit <= 0)
			break;

		localIt = localIt.getParent();
	}

	if (aXFormAttributes.empty())
	{
		FnKat::GroupBuilder gb;
		return gb.build();
	}

	FnKat::GroupBuilder gb;

	unsigned int count = 0;
	char szTemp[16];
	std::vector<FnKat::GroupAttribute>::reverse_iterator itItem = aXFormAttributes.rbegin();
	for (; itItem != aXFormAttributes.rend(); ++itItem)
	{
		sprintf(szTemp, "item %i", count++);

		gb.set(szTemp, *itItem);
	}

	return gb.build();
}

Foundry::Katana::RenderOutputUtils::XFormMatrixVector KatanaHelpers::getXFormMatrixStatic(FnKat::GroupAttribute xformAttr)
{
	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	std::pair<FnAttribute::DoubleAttribute, bool> res = Foundry::Katana::FnXFormUtil::CalcTransformMatrixAtTime(xformAttr, 0.0f);

#ifdef KATANA_V3
	FnAttribute::DoubleAttribute::array_type data = res.first.getNearestSample(0.0f);
	FnKat::RenderOutputUtils::XFormMatrix matrix(data.data());
#else
	FnKat::RenderOutputUtils::XFormMatrix matrix(res.first.getNearestSample(0.0f).data());
#endif
	xforms.push_back(matrix);

	return xforms;
}

Foundry::Katana::RenderOutputUtils::XFormMatrixVector KatanaHelpers::getXFormMatrixStatic(FnKat::FnScenegraphIterator iterator)
{
	FnKat::GroupAttribute xformAttr = iterator.getGlobalXFormGroup();

	return getXFormMatrixStatic(xformAttr);
}

Foundry::Katana::RenderOutputUtils::XFormMatrixVector KatanaHelpers::getXFormMatrixMB(FnKat::FnScenegraphIterator iterator,
																					  bool clampWithinShutter, float shutterOpen, float shutterClose)
{
	FnKat::GroupAttribute xformAttr = iterator.getGlobalXFormGroup();

	FnAttribute::DoubleAttribute matrix = FnAttribute::RemoveTimeSamplesIfAllSame(
			FnAttribute::RemoveTimeSamplesUnneededForShutter(
				FnGeolibServices::FnXFormUtil::CalcTransformMatrixAtExistingTimes(xformAttr).first,
				shutterOpen, shutterClose));

	Foundry::Katana::RenderOutputUtils::XFormMatrixVector finalValues;
	for (unsigned int i = 0; i < matrix.getNumberOfTimeSamples(); i++)
	{
		float sampleTime = matrix.getSampleTime(i);
		FnKat::DoubleConstVector matrixValues = matrix.getNearestSample(sampleTime);

		Foundry::Katana::RenderOutputUtils::XFormMatrix newM(matrixValues.data());
		finalValues.push_back(newM);
	}

	return finalValues;
}

void KatanaHelpers::getRelevantSampleTimes(FnKat::DataAttribute attribute, std::vector<float>& aSampleTimes, float shutterOpen, float shutterClose)
{
	// we need to do this ourself...
	std::vector<float> attributeSampleTimes;
	unsigned int numAttributeSampleTimes = attribute.getNumberOfTimeSamples();
	for (unsigned int i = 0; i < numAttributeSampleTimes; i++)
	{
		float sampleTime = attribute.getSampleTime(i);
		attributeSampleTimes.push_back(sampleTime);
	}

	std::set<float> sampleTimes(attributeSampleTimes.begin(), attributeSampleTimes.end());

	FnKat::RenderOutputUtils::findSampleTimesRelevantToShutterRange(aSampleTimes, sampleTimes, shutterOpen, shutterClose);
}


//

KatanaAttributeHelper::KatanaAttributeHelper(const FnKat::GroupAttribute& attribute) : m_attribute(attribute)
{

}


float KatanaAttributeHelper::getFloatParam(const std::string& name, float defaultValue) const
{
	return getFloatParam(m_attribute, name, defaultValue);
}

int KatanaAttributeHelper::getIntParam(const std::string& name, int defaultValue) const
{
	return getIntParam(m_attribute, name, defaultValue);
}

Colour3f KatanaAttributeHelper::getColourParam(const std::string& name, const Colour3f& defaultValue) const
{
	return getColourParam(m_attribute, name, defaultValue);
}

std::string KatanaAttributeHelper::getStringParam(const std::string& name, const std::string& defaultValue) const
{
	return getStringParam(m_attribute, name, defaultValue);
}


// static versions
float KatanaAttributeHelper::getFloatParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, float defaultValue)
{
	FnKat::FloatAttribute floatAttrib = shaderParamsAttr.getChildByName(name);

	if (!floatAttrib.isValid())
		return defaultValue;

	return floatAttrib.getValue(defaultValue, false);
}

int KatanaAttributeHelper::getIntParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, int defaultValue)
{
	FnKat::IntAttribute intAttrib = shaderParamsAttr.getChildByName(name);

	if (!intAttrib.isValid())
		return defaultValue;

	return intAttrib.getValue(defaultValue, false);
}

Colour3f KatanaAttributeHelper::getColourParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name, const Colour3f& defaultValue)
{
	FnKat::FloatAttribute floatAttrib = shaderParamsAttr.getChildByName(name);

	if (!floatAttrib.isValid())
		return defaultValue;

	if (floatAttrib.getTupleSize() != 3)
		return defaultValue;

	FnKat::FloatConstVector data = floatAttrib.getNearestSample(0);

	Colour3f returnValue;
	returnValue.r = data[0];
	returnValue.g = data[1];
	returnValue.b = data[2];

	return returnValue;
}

std::string KatanaAttributeHelper::getStringParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name,
												  const std::string& defaultValue)
{
	FnKat::StringAttribute stringAttrib = shaderParamsAttr.getChildByName(name);

	if (!stringAttrib.isValid())
		return defaultValue;

	return stringAttrib.getValue(defaultValue, false);
}
