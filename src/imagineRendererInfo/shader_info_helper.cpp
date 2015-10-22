#include "shader_info_helper.h"

#include <vector>

#ifdef KAT_V_2
#include <FnRendererInfo/plugin/RendererInfoBase.h>
#else
#include <RendererInfo/RendererInfoBase.h>
#endif

#include <FnAttribute/FnDataBuilder.h>

#include "imagine_renderer_info.h"

// TODO: Hardcode this stuff for the moment, as we haven't got a nice API for doing this yet...
//       We *could* just instantiate Imagine's materials and call buildParams() on them and then build the
//       list dynamically from the params which would be practically the same, but would be a bit messy,
//       and there's no current way of getting defaults, so...

// RenderInfoBase stupidly has these marked as protected, so redefine them here so we can use them easily...
typedef std::pair<std::string, int> EnumPair;
typedef std::vector<EnumPair> EnumPairVector;

const char* falloffTypeOptions[] = { "none", "linear", "quadratic", 0 };
const char* shadowTypeOptions[] = { "normal", "transparent", "none", 0 };
static const char* areaShapeTypeOptions[] = { "quad", "disc", "sphere", "cylinder", 0 };

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

	typeTags.push_back("surface");
	typeTags.push_back("bump");
	typeTags.push_back("medium");
	typeTags.push_back("displacement");
	typeTags.push_back("alpha");
	typeTags.push_back("light");

	Foundry::Katana::RendererInfo::RendererInfoBase::configureBasicRenderObjectInfo(rendererObjectInfo, kFnRendererObjectTypeShader,
																					typeTags, location, fullPath,
																					kFnRendererObjectValueTypeUnknown, containerHintsAttribute);

	if (name == "Standard")
	{
		buildStandardShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "StandardImage")
	{
		buildStandardImageShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Glass")
	{
		buildGlassShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Metal")
	{
		buildMetalShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Plastic")
	{
		buildPlasticShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Brushed Metal")
	{
		buildBrushedMetalShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Metallic Paint")
	{
		buildMetallicPaintShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Translucent")
	{
		buildTranslucentShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Velvet")
	{
		buildVelvetShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Luminous")
	{
		buildLuminousShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Wireframe")
	{
		buildWireframeShaderParams(iri, rendererObjectInfo);
	}
	// lights
	else if (name == "Point")
	{
		buildPointLightShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Spot")
	{
		buildSpotLightShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Area")
	{
		buildAreaLightShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Distant")
	{
		buildDistantLightShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "SkyDome")
	{
		buildSkydomeLightShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "Environment")
	{
		buildEnvironmentLightShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "PhysicalSky")
	{
		buildPhysicalSkyLightShaderParams(iri, rendererObjectInfo);
	}


	//
	else if (name == "ImageTextureBump")
	{
		buildBumpTextureShaderParams(iri, rendererObjectInfo);
	}
	else if (name == "ImageTextureAlpha")
	{
		buildAlphaTextureShaderParams(iri, rendererObjectInfo);
	}

	return true;
}

void ShaderInfoHelper::buildStandardShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("diff_col", Col3f(0.8f, 0.8f, 0.8f));
	helper.addStringParam("diff_col_texture");
	helper.addIntParam("diff_col_texture_flags", 0);
	helper.addFloatSliderParam("diff_roughness", 0.0f);
	helper.addFloatSliderParam("diff_backlit", 0.0f);

	helper.addColourParam("spec_col", Col3f(0.1f, 0.1f, 0.1f));
	helper.addStringParam("spec_col_texture");
	helper.addIntParam("spec_col_texture_flags", 0);
	helper.addFloatSliderParam("spec_roughness", 0.9f);

	helper.addFloatSliderParam("reflection", 0.0f);
	helper.addFloatSliderParam("reflection_roughness", 0.0f);

	helper.addBoolParam("fresnel", false);
	helper.addFloatSliderParam("fresnel_coef", 0.0f);

	helper.addFloatParam("refraction_index", 1.0f);

	helper.addFloatSliderParam("transparency", 0.0f);
	helper.addFloatSliderParam("transmission", 0.0f);

	helper.addBoolParam("double_sided", 0);
}

void ShaderInfoHelper::buildStandardImageShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("diff_col", Col3f(0.8f, 0.8f, 0.8f));
	helper.addStringParam("diff_col_texture");
	helper.addIntParam("diff_col_texture_flags", 0);

	helper.addFloatSliderParam("diff_roughness", 0.0f);
	helper.addStringParam("diff_roughness_texture");

	helper.addFloatSliderParam("diff_backlit", 0.0f);
	helper.addStringParam("diff_backlit_texture");

	helper.addColourParam("spec_col", Col3f(0.1f, 0.1f, 0.1f));
	helper.addStringParam("spec_col_texture");
	helper.addIntParam("spec_col_texture_flags", 0);

	helper.addFloatSliderParam("spec_roughness", 0.9f);
	helper.addStringParam("spec_roughness_texture");

	helper.addFloatSliderParam("reflection", 0.0f);
	helper.addFloatSliderParam("reflection_roughness", 0.0f);

	helper.addBoolParam("fresnel", false);
	helper.addFloatSliderParam("fresnel_coef", 0.0f);

	helper.addFloatParam("refraction_index", 1.0f);

	helper.addFloatSliderParam("transparency", 0.0f);
	helper.addFloatSliderParam("transmission", 0.0f);

	helper.addBoolParam("double_sided", 0);
}

void ShaderInfoHelper::buildGlassShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(0.0f, 0.0f, 0.0f));
	helper.addFloatSliderParam("reflection", 1.0f);
	helper.addFloatSliderParam("roughness", 0.0f);
	helper.addFloatSliderParam("transparency", 1.0f);
	helper.addFloatSliderParam("transmittance", 1.0f);

	helper.addFloatParam("refraction_index", 1.517f);

	helper.addBoolParam("fresnel", true);
	helper.addBoolParam("thin_volume", false);
	helper.addBoolParam("ignore_trans_refraction", false);
}

void ShaderInfoHelper::buildMetalShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(0.9f, 0.9f, 0.9f));
	helper.addFloatParam("refraction_index", 1.39f);
	helper.addFloatParam("k", 4.8f);
	helper.addFloatSliderParam("roughness", 0.01f);

	helper.addBoolParam("double_sided", 0);
}

void ShaderInfoHelper::buildPlasticShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(0.5f, 0.5f, 1.0f));
	helper.addFloatSliderParam("roughness", 0.1f);
	helper.addFloatSliderParam("fresnel_coef", 0.0f);
	helper.addFloatParam("refraction_index", 1.39f);
}

void ShaderInfoHelper::buildBrushedMetalShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(0.9f, 0.9f, 0.9f));
	helper.addFloatParam("refraction_index", 1.39f);
	helper.addFloatParam("k", 4.8f);
	helper.addFloatSliderParam("roughness_x", 0.1f);
	helper.addFloatSliderParam("roughness_y", 0.02f);

	helper.addBoolParam("double_sided", 0);
}

void ShaderInfoHelper::buildMetallicPaintShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(0.29f, 0.016f, 0.019f));
	helper.addStringParam("col_texture");
	helper.addColourParam("flake_colour", Col3f(0.39, 0.016f, 0.19f));
	helper.addFloatSliderParam("flake_spread", 0.32f);
	helper.addFloatSliderParam("flake_mix", 0.38f);
	helper.addFloatParam("refraction_index", 1.39f);

	helper.addFloatSliderParam("reflection", 1.0f);
	helper.addFloatSliderParam("fresnel_coef", 0.0f);
}

void ShaderInfoHelper::buildTranslucentShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("surface_col", Col3f(0.7f, 0.7f, 0.7f));
	helper.addFloatParam("surface_roughness", 0.05f);

	helper.addColourParam("specular_col", Col3f(0.1f, 0.1f, 0.1f));

	helper.addColourParam("inner_col", Col3f(0.4f, 0.4f, 0.4f));
	helper.addFloatSliderParam("subsurface_density", 3.1f, 0.0001f, 10.0f);
	helper.addFloatSliderParam("sampling_density", 0.35f, 0.001f, 2.0f);

	helper.addFloatSliderParam("transmittance", 0.41f);

	helper.addFloatSliderParam("absorption_ratio", 0.46f);

	helper.addFloatParam("refractionIndex", 1.42f);
}

void ShaderInfoHelper::buildVelvetShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("horiz_col", Col3f(0.7f, 0.7f, 0.7f));
	helper.addStringParam("horiz_col_texture");
	helper.addFloatSliderParam("horiz_scatter_falloff", 0.4f);

	helper.addColourParam("backscatter_col", Col3f(0.4f, 0.4f, 0.4f));
	helper.addStringParam("backscatter_col_texture");
	helper.addFloatSliderParam("backscatter", 0.7f);
}

void ShaderInfoHelper::buildWireframeShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("interior_colour", Col3f(0.7f, 0.7f, 0.7f));
	helper.addFloatSliderParam("line_width", 0.005f, 0.0f, 10.0f);

	helper.addColourParam("line_colour", Col3f(0.01f, 0.01f, 0.01f));
	helper.addFloatSliderParam("edge_softness", 0.3f);

	helper.addIntParam("edge_type", 1);
}

void ShaderInfoHelper::buildLuminousShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(1.0f, 1.0f, 1.0f));
	helper.addFloatSliderParam("intensity", 1.0f, 0.0f, 20.0f);

	helper.addBoolParam("register_as_light", 0);

	helper.addFloatSliderParam("light_intensity", 1.0f, 0.0f, 10.0f);
	helper.addFloatSliderParam("light_exposure", 3.0f, 0.0f, 20.0f);

	helper.addBoolParam("light_quadratic_falloff", 1);
	helper.addBoolParam("light_weight_by_surface_area", 1);
}

void ShaderInfoHelper::buildCommonLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo,
													bool addColour, bool visibleOn)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("intensity", 1.0f, 0.0f, 20.0f);
	helper.addFloatSliderParam("exposure", 0.0f, 0.0f, 20.0f);
	if (addColour)
	{
		helper.addColourParam("colour", Col3f(1.0f, 1.0f, 1.0f));
	}

	helper.addStringPopupParam("shadow_type", "normal", shadowTypeOptions, 3);
	helper.addIntParam("num_samples", 1);

	helper.addBoolParam("visible", visibleOn);
}

void ShaderInfoHelper::buildPointLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true, false);

	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addStringPopupParam("falloff", "quadratic", falloffTypeOptions, 3);
}

void ShaderInfoHelper::buildSpotLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	buildCommonLightShaderParams(iri, rendererObjectInfo, true, true);

	helper.addStringPopupParam("falloff", "quadratic", falloffTypeOptions, 3);

	helper.addFloatSliderParam("cone_angle", 30.0f, 0.0f, 90.0f);
	helper.addFloatSliderParam("penumbra_angle", 5.0f, 0.0f, 90.0f);

	helper.addBoolParam("is_area", true);
}

void ShaderInfoHelper::buildAreaLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true, false);

	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("width", 1.0f, 0.01f, 20.0f);
	helper.addFloatSliderParam("depth", 1.0f, 0.01f, 20.0f);

	helper.addStringPopupParam("shape_type", "quad", areaShapeTypeOptions, 4);

	helper.addStringPopupParam("falloff", "quadratic", falloffTypeOptions, 3);

	helper.addBoolParam("scale", true);
	helper.addBoolParam("bi-directional", false);
}

void ShaderInfoHelper::buildDistantLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true, false);

	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("spread_angle", 1.0f, 0.0f, 33.0f);
}

void ShaderInfoHelper::buildSkydomeLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true, false);
}

void ShaderInfoHelper::buildEnvironmentLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("intensity", 1.0f, 0.0f, 5.0f);
	helper.addFloatSliderParam("exposure", 0.0f, 0.0f, 20.0f);
	helper.addStringPopupParam("shadow_type", "normal", shadowTypeOptions, 3);
	helper.addIntParam("num_samples", 1);
	helper.addBoolParam("visible", true);

	helper.addStringParam("env_map_path");
}

void ShaderInfoHelper::buildPhysicalSkyLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatParam("intensity", 1.0f);
	helper.addFloatSliderParam("exposure", 0.0f, 0.0f, 20.0f);
	helper.addStringPopupParam("shadow_type", "normal", shadowTypeOptions, 3);
	helper.addIntParam("num_samples", 1);
	helper.addBoolParam("visible", true);

	helper.addFloatSliderParam("sky_intensity", 1.0f, 0.0f, 5.0f);
	helper.addFloatSliderParam("sun_intensity", 1.0f, 0.0f, 5.0f);

	helper.addIntParam("hemisphere_extension", 0);

	helper.addFloatSliderParam("turbidity", 3.0f, 0.001f, 20.0f);

	helper.addFloatSliderParam("time", 17.1f, 0.0f, 24.0f);
	helper.addIntParam("day", 174);
}

void ShaderInfoHelper::buildBumpTextureShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addStringParam("bump_texture_path");
	helper.addFloatSliderParam("bump_texture_intensity", 0.8f, -4.0f, 4.0f);
}

void ShaderInfoHelper::buildAlphaTextureShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addStringParam("alpha_texture_path");
	helper.addBoolParam("alpha_texture_invert", false);
}

//

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

void ShaderInfoHelper::addFloatSliderParam(const std::string& name, float defaultValue, float sliderMin, float sliderMax)
{
	EnumPairVector enums;

	FnKat::FloatBuilder fb(1);
	fb.push_back(defaultValue);

	FnKat::Attribute defaultAttribute = fb.build();
	FnKat::GroupBuilder params;
	params.set("isDynamicArray", FnKat::IntAttribute(0));
	params.set("slider", FnKat::IntAttribute(1));
	params.set("slidermin", FnKat::FloatAttribute(sliderMin));
	params.set("slidermax", FnKat::FloatAttribute(sliderMax));

	// TODO: see if we can work out how to get log/exp sliders for roughness params...

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
	params.set("panelWidget", FnKat::StringAttribute("color"));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeColor3, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addIntParam(const std::string& name, int defaultValue)
{
	EnumPairVector enums;

	FnKat::IntBuilder ib(1);
	ib.push_back(defaultValue);

	FnKat::Attribute defaultAttribute = ib.build();
	FnKat::GroupBuilder params;

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeInt, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addIntEnumParam(const std::string& name, int defaultValue, const char** options, unsigned int size)
{
	EnumPairVector enums;

	FnKat::IntBuilder ib(1);
	ib.push_back(defaultValue);

	FnKat::Attribute defaultAttribute = ib.build();
	FnKat::GroupBuilder params;

	params.set("options", FnKat::StringAttribute(options, size, 1));
	params.set("widget", FnKat::StringAttribute("popup"));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeInt, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addBoolParam(const std::string& name, bool defaultValue)
{
	EnumPairVector enums;

	FnKat::IntBuilder ib(1);
	ib.push_back((int)defaultValue);

	FnKat::Attribute defaultAttribute = ib.build();
	FnKat::GroupBuilder params;

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeBoolean, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addStringParam(const std::string& name)
{
	EnumPairVector enums;

	FnKat::StringBuilder sb(1);
	sb.push_back("");

	FnKat::Attribute defaultAttribute = sb.build();
	FnKat::GroupBuilder params;
	params.set("widget", FnKat::StringAttribute("assetIdInput"));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeString, 0, defaultAttribute, hintsAttribute, enums);
}

void ShaderInfoHelper::addStringPopupParam(const std::string& name, const std::string& defaultValue, const char** options, unsigned int size)
{
	EnumPairVector enums;

	FnKat::StringBuilder sb(1);
	sb.push_back(defaultValue);

	FnKat::Attribute defaultAttribute = sb.build();
	FnKat::GroupBuilder params;

	params.set("options", FnKat::StringAttribute(options, size, 1));
	params.set("isDynamicArray", FnKat::IntAttribute(0));
	params.set("widget", FnKat::StringAttribute("popup"));

	FnKat::Attribute hintsAttribute = params.build();

	m_iri.localAddRenderObjectParam(m_rendererObjectInfo, name, kFnRendererObjectValueTypeString, 0, defaultAttribute, hintsAttribute, enums);
}
