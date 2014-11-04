#include "katana_helpers.h"

#include <stdio.h>

#include <set>

#include <FnAttribute/FnGroupBuilder.h>

#ifdef KAT_V_2
#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#include <FnGeolibServices/FnXFormUtil.h>
#else
#include <RenderOutputUtils/RenderOutputUtils.h>
#endif

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

#ifdef KAT_V_2
	std::pair<FnAttribute::DoubleAttribute, bool> res = Foundry::Katana::FnXFormUtil::CalcTransformMatrixAtTime(xformAttr, 0.0f);

	FnKat::RenderOutputUtils::XFormMatrix matrix(res.first.getNearestSample(0.0f).data());
	xforms.push_back(matrix);
#else
	std::vector<float> relevantSampleTimes;
	relevantSampleTimes.push_back(0.0f);

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);
#endif

	return xforms;
}

Foundry::Katana::RenderOutputUtils::XFormMatrixVector KatanaHelpers::getXFormMatrixStatic(FnKat::FnScenegraphIterator iterator)
{
#ifdef KAT_V_2
	FnKat::GroupAttribute xformAttr = iterator.getGlobalXFormGroup();

	return getXFormMatrixStatic(xformAttr);

#else
	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::vector<float> relevantSampleTimes;
	relevantSampleTimes.push_back(0.0f);

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	return xforms;
#endif
}

Foundry::Katana::RenderOutputUtils::XFormMatrixVector KatanaHelpers::getXFormMatrixMB(FnKat::FnScenegraphIterator iterator,
																					  bool clampWithinShutter, float shutterOpen, float shutterClose)
{
#ifdef KAT_V_2


#else
	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	bool isCoherentSampleSet = true;
	std::set<float> sampleTimes;

	FnKat::RenderOutputUtils::findAllMotionRelevantTimesForGroupAttr(sampleTimes, isCoherentSampleSet, xformAttr);
	if (sampleTimes.empty())
	{
		sampleTimes.insert(0.0f);
	}

	if (!isCoherentSampleSet)
	{
		// print warning?
	}

	std::vector<float> relevantSampleTimes;

	if (clampWithinShutter)
	{
		FnKat::RenderOutputUtils::findSampleTimesRelevantToShutterRange(relevantSampleTimes, sampleTimes, shutterOpen, shutterClose);
	}
	else
	{
		std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));
	}

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	return xforms;
#endif
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

std::string KatanaAttributeHelper::getStringParam(const std::string& name) const
{
	return getStringParam(m_attribute, name);
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

std::string KatanaAttributeHelper::getStringParam(const FnKat::GroupAttribute& shaderParamsAttr, const std::string& name)
{
	FnKat::StringAttribute stringAttrib = shaderParamsAttr.getChildByName(name);

	if (!stringAttrib.isValid())
		return "";

	return stringAttrib.getValue("", false);
}
