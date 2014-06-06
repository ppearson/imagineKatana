#include "shader_info_helper.h"

#include <vector>

#include <RendererInfo/RendererInfoBase.h>
#include <FnAttribute/FnDataBuilder.h>

#include "imagine_renderer_info.h"

// TODO: Hardcode this stuff for the moment, as we haven't got a nice API for doing this yet...
//       We *could* just instantiate Imagine's materials and call buildParams() on them and then build the
//       list dynamically from the params which would be practically the same, but would be a bit messy, so...

// RenderInfoBase stupidly has these marked as protected, so redefine them here so we can use them easily...
typedef std::pair<std::string, int> EnumPair;
typedef std::vector<EnumPair> EnumPairVector;

ShaderInfoHelper::ShaderInfoHelper(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo) : m_iri(iri),
	m_rendererObjectInfo(rendererObjectInfo)
{
}

void ShaderInfoHelper::fillShaderInputNames(const std::string& shaderName, std::vector<std::string>& names)
{
	if (shaderName == "Standard")
	{
		names.push_back("diff_col");
		names.push_back("diff_roughness");
		names.push_back("diff_backlit");
		names.push_back("spec_col");
		names.push_back("spec_type");
		names.push_back("spec_roughness");
		names.push_back("reflection");
		names.push_back("fresnel");
		names.push_back("fresnel_coef");
		names.push_back("transparency");
		names.push_back("transmittance");

		names.push_back("double_sided");
	}
}

bool ShaderInfoHelper::buildShaderInfo(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
								  const FnKat::GroupAttribute inputAttr)
{
	std::vector<std::string> typeTags;
	std::string location = name;
	std::string fullPath;

	FnKat::Attribute containerHintsAttribute;

	Foundry::Katana::RendererInfo::RendererInfoBase::configureBasicRenderObjectInfo(rendererObjectInfo, kFnRendererObjectTypeShader,
																					typeTags, location, fullPath,
																					kFnRendererObjectValueTypeUnknown, containerHintsAttribute);

	if (name == "Standard")
	{
		buildStandardShaderParams(iri, rendererObjectInfo);
	}

	return true;
}

void ShaderInfoHelper::buildStandardShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("diffuse_col", Col3f(0.8f, 0.8f, 0.8f));
	helper.addFloatParam("diff_roughness", 0.0f);
	helper.addFloatParam("diff_backlit", 0.0f);

	helper.addColourParam("spec_col", Col3f(0.1f, 0.1f, 0.1f));
	helper.addFloatParam("spec_roughness", 0.9f);

	helper.addFloatParam("reflection", 0.0f);

	helper.addBoolParam("fresnel", false);
	helper.addFloatParam("fresnel_coef", 0.0f);

	helper.addFloatParam("refraction_index", 1.0f);

	helper.addFloatParam("transparency", 0.0f);
	helper.addFloatParam("transmission", 0.0f);
}

void ShaderInfoHelper::addFloatParam(const std::string& name, float defaultValue)
{
	EnumPairVector enums;

	FnKat::FloatBuilder fb(1);
	fb.push_back(defaultValue);

	FnKat::Attribute defaultAttribute = fb.build();
	FnKat::GroupBuilder params;
	params.set("isDynamicArray", FnKat::IntAttribute(0));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeFloat, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addColourParam(const std::string& name, Col3f defaultValue)
{
	EnumPairVector enums;

	FnKat::FloatBuilder fb(3);
	fb.push_back(defaultValue.r);
	fb.push_back(defaultValue.g);
	fb.push_back(defaultValue.b);

	FnKat::Attribute defaultAttribute = fb.build();
	FnKat::GroupBuilder params;
	params.set("widget", FnKat::StringAttribute("dynamicArray"));
	params.set("panelWidget", FnKat::StringAttribute("color"));
	params.set("isDynamicArray", FnKat::IntAttribute(1));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeColor3, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addBoolParam(const std::string& name, bool defaultValue)
{
	EnumPairVector enums;

	FnKat::IntBuilder ib(1);
	ib.push_back((int)defaultValue);

	FnKat::Attribute defaultAttribute = ib.build();
	FnKat::GroupBuilder params;
	params.set("isDynamicArray", FnKat::IntAttribute(0));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeBoolean, 0, defaultAttribute, hintsAttribute, enums);
}
