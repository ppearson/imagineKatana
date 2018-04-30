#include "shader_info_helper.h"

#include <vector>

#include <FnRendererInfo/plugin/RendererInfoBase.h>

#include <FnAttribute/FnDataBuilder.h>

#include "imagine_renderer_info.h"

// TODO: Hardcode this stuff for the moment, as we haven't got a nice API for doing this yet...
//       We *could* just instantiate Imagine's materials and call buildParams() on them and then build the
//       list dynamically from the params which would be practically the same, but would be a bit messy,
//       and there's no current way of getting defaults, so...

// RenderInfoBase stupidly has these marked as protected, so redefine them here so we can use them easily...
typedef std::pair<std::string, int> EnumPair;
typedef std::vector<EnumPair> EnumPairVector;

static const char* falloffTypeOptions[] = { "none", "linear", "quadratic", 0 };
static const char* shadowTypeOptions[] = { "normal", "transparent", "none", 0 };
static const char* areaShapeTypeOptions[] = { "quad", "disc", "sphere", "cylinder", 0 };
static const char* swatchGridTypeOptions[] = { "none", "hue", "fixed colour", 0 };
static const char* microfacetTypeOptions[] = { "phong", "beckmann", "ggx", 0 };
static const char* translucentSurfaceTypeOptions[] = { "diffuse", "dielectric layer", "transmission only", 0 };
static const char* translucentScatterModeOptions[] = { "legacy", "mean free path", 0 };
static const char* translucentEntryExitTypeOptions[] = { "refractive fresnel", "refractive", "transmissive", 0 };

ShaderInfoHelper::ShaderInfoHelper(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo) : m_iri(iri),
	m_rendererObjectInfo(rendererObjectInfo)
{
}

void ShaderInfoHelper::fillShaderInputNames(const std::string& shaderName, std::vector<std::string>& names)
{
	if (shaderName == "Shader/Standard")
	{
		names.push_back("diff_col");
		names.push_back("diff_roughness");
		names.push_back("diff_backlit");
		names.push_back("spec_col");
		names.push_back("spec_roughness");
		names.push_back("reflection");
		names.push_back("transparency");
		names.push_back("transmittance");
	}
	else if (shaderName == "Op/Adjust")
	{
		names.push_back("input");
	}
	else if (shaderName == "Op/Mix")
	{
		names.push_back("mix_amount");
		names.push_back("input_A");
		names.push_back("input_B");
	}
}

void ShaderInfoHelper::fillShaderOutputNames(const std::string& shaderName, std::vector<std::string>& names)
{

}

bool ShaderInfoHelper::buildShaderInfo(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
								  const FnKat::GroupAttribute inputAttr)
{
	std::vector<std::string> typeTags;
	std::string location = name;
	std::string fullPath;

	FnKat::Attribute containerHintsAttribute;

	typeTags.push_back("surface");
	typeTags.push_back("op");
	typeTags.push_back("texture");
	typeTags.push_back("bump");
	typeTags.push_back("medium");
	typeTags.push_back("displacement");
	typeTags.push_back("alpha");
	typeTags.push_back("light");

	Foundry::Katana::RendererInfo::RendererInfoBase::configureBasicRenderObjectInfo(rendererObjectInfo, kFnRendererObjectTypeShader,
																					typeTags, location, fullPath,
																					kFnRendererObjectValueTypeUnknown, containerHintsAttribute);

	std::string buildName;
	std::string typeName; // for network materials
	if (name.find("/") == std::string::npos)
	{
		// just a single standard material
		buildName = name;
	}
	else
	{
		// ImagineShadingNode item, so split out the two items from the name
		size_t sepPos = name.find("/");
		typeName = name.substr(0, sepPos);
		buildName = name.substr(sepPos + 1);
	}

	if (buildName == "Standard")
	{
		buildStandardShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "StandardImage")
	{
		buildStandardImageShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Glass")
	{
		buildGlassShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Metal")
	{
		buildMetalShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Plastic")
	{
		buildPlasticShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Brushed Metal")
	{
		buildBrushedMetalShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Metallic Paint")
	{
		buildMetallicPaintShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Translucent")
	{
		buildTranslucentShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Velvet")
	{
		buildVelvetShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Luminous")
	{
		buildLuminousShaderParams(iri, rendererObjectInfo);
	}
	// lights
	else if (buildName == "Point")
	{
		buildPointLightShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Spot")
	{
		buildSpotLightShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Area")
	{
		buildAreaLightShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Distant")
	{
		buildDistantLightShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "SkyDome")
	{
		buildSkydomeLightShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "Environment")
	{
		buildEnvironmentLightShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "PhysicalSky")
	{
		buildPhysicalSkyLightShaderParams(iri, rendererObjectInfo);
	}


	//
	else if (buildName == "ImageTextureBump")
	{
		buildBumpTextureShaderParams(iri, rendererObjectInfo);
	}
	else if (buildName == "ImageTextureAlpha")
	{
		buildAlphaTextureShaderParams(iri, rendererObjectInfo);
	}

	if (typeName == "Texture")
	{
		if (buildName == "Constant")
		{
			buildConstantTextureParams(iri, rendererObjectInfo);
		}
		else if (buildName == "Checkerboard")
		{
			buildCheckerboardTextureParams(iri, rendererObjectInfo);
		}
		else if (buildName == "Grid")
		{
			buildGridTextureParams(iri, rendererObjectInfo);
		}
		else if (buildName == "Swatch")
		{
			buildSwatchTextureParams(iri, rendererObjectInfo);
		}
		else if (buildName == "TextureRead")
		{
			buildWireframeTextureParams(iri, rendererObjectInfo);
		}
		else if (buildName == "Wireframe")
		{
			buildWireframeTextureParams(iri, rendererObjectInfo);
		}
	}
	else if (typeName == "Op")
	{
		if (buildName == "Adjust")
		{
			buildAdjustOpParams(iri, rendererObjectInfo);
		}
		else if (buildName == "Mix")
		{
			buildMixOpParams(iri, rendererObjectInfo);
		}
	}

	return true;
}

void ShaderInfoHelper::setShaderParameterMapping(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& metaName,
									  const std::string& actualName)
{
	iri.localSetShaderParameterMapping(rendererObjectInfo, metaName, actualName);
}


void ShaderInfoHelper::buildStandardShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("diff_col", Col3f(0.6f, 0.6f, 0.6f));
	helper.addStringParam("diff_col_texture");
	helper.addFloatSliderParam("diff_roughness", 0.0f);
	helper.addFloatSliderParam("diff_backlit", 0.0f);

	helper.addColourParam("spec_col", Col3f(0.0f, 0.0f, 0.0f));
	helper.addStringParam("spec_col_texture");
	helper.addFloatSliderParam("spec_roughness", 0.15f);

	helper.addStringPopupParam("microfacet_type", "beckmann", microfacetTypeOptions, 3);

	helper.addFloatSliderParam("reflection", 0.0f);
	helper.addFloatSliderParam("reflection_roughness", 0.0f);

	helper.addBoolParam("fresnel", true);
	helper.addFloatSliderParam("fresnel_coef", 0.0f);

	helper.addFloatParam("refraction_index", 1.49f);

	helper.addFloatSliderParam("transparency", 0.0f);
	helper.addFloatSliderParam("transmission", 0.0f);

	helper.addBoolParam("double_sided", 0);
}
					// for the moment, hopefully we're only going to be connecting Ops
void ShaderInfoHelper::buildStandardImageShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("diff_col", Col3f(0.6f, 0.6f, 0.6f));
	helper.addStringParam("diff_col_texture");

	helper.addFloatSliderParam("diff_roughness", 0.0f);
	helper.addStringParam("diff_roughness_texture");

	helper.addFloatSliderParam("diff_backlit", 0.0f);
	helper.addStringParam("diff_backlit_texture");

	helper.addColourParam("spec_col", Col3f(0.0f, 0.0f, 0.0f));
	helper.addStringParam("spec_col_texture");

	helper.addFloatSliderParam("spec_roughness", 0.15f);
	helper.addStringParam("spec_roughness_texture");

	helper.addStringPopupParam("microfacet_type", "beckmann", microfacetTypeOptions, 3);

	helper.addFloatSliderParam("reflection", 0.0f);
	helper.addFloatSliderParam("reflection_roughness", 0.0f);

	helper.addBoolParam("fresnel", true);
	helper.addFloatSliderParam("fresnel_coef", 0.0f);

	helper.addFloatParam("refraction_index", 1.49f);

	helper.addFloatSliderParam("transparency", 0.0f);
	helper.addFloatSliderParam("transmittance", 1.0f);

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

	helper.addStringPopupParam("microfacet_type", "beckmann", microfacetTypeOptions, 3);

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

	helper.addColourParam("surface_col", Col3f(0.0f, 0.0f, 0.0f));
	helper.addStringParam("surface_col_texture");

	helper.addColourParam("specular_col", Col3f(0.1f, 0.1f, 0.1f));
	helper.addStringParam("specular_col_texture");
	helper.addFloatSliderParam("specular_roughness", 0.05f);

	helper.addStringPopupParam("surface_type", "diffuse", translucentSurfaceTypeOptions, 3);
	helper.addStringPopupParam("scatter_mode", "mean free path", translucentScatterModeOptions, 2);

	helper.addColourParam("inner_col", Col3f(0.4f, 0.4f, 0.4f));
	helper.addFloatSliderParam("subsurface_density", 3.1f, 0.0001f, 10.0f);

	helper.addColourParam("mfp", Col3f(0.22f, 0.081f, 0.06f));
	helper.addFloatSliderParam("mfp_scale", 2.7f, 0.001f, 25.0f);
	helper.addColourParam("scatter_albedo", Col3f(0.3f, 0.3f, 0.3f));

	helper.addFloatSliderParam("sampling_density", 0.65f, 0.0001f, 2.0f);

	helper.addIntParam("scatter_limit", 6);

	helper.addBoolParam("use_surf_col_as_trans", false);

	helper.addFloatSliderParam("transmittance", 1.0f);
	helper.addFloatSliderParam("transmittance_roughness", 0.8f);

	helper.addStringPopupParam("entry_exit_type", "refractive", translucentEntryExitTypeOptions, 3);

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
													bool addColour)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("intensity", 1.0f, 0.0f, 100.0f);
	helper.addFloatSliderParam("exposure", 0.0f, 0.0f, 25.0f);
	if (addColour)
	{
		helper.addColourParam("colour", Col3f(1.0f, 1.0f, 1.0f));
	}

	helper.addStringPopupParam("shadow_type", "normal", shadowTypeOptions, 3);
	helper.addIntParam("num_samples", 1);

	setShaderParameterMapping(iri, rendererObjectInfo, "shader", "imagineLightShader");
	if (addColour)
	{
		setShaderParameterMapping(iri, rendererObjectInfo, "color", "imagineLightParams.colour");
	}

	setShaderParameterMapping(iri, rendererObjectInfo, "intensity", "imagineLightParams.intensity");
	setShaderParameterMapping(iri, rendererObjectInfo, "exposure", "imagineLightParams.exposure");
}

void ShaderInfoHelper::buildPointLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true);

	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addStringPopupParam("falloff", "quadratic", falloffTypeOptions, 3);
}

void ShaderInfoHelper::buildSpotLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	buildCommonLightShaderParams(iri, rendererObjectInfo, true);

	helper.addStringPopupParam("falloff", "quadratic", falloffTypeOptions, 3);

	helper.addFloatSliderParam("cone_angle", 30.0f, 0.0f, 90.0f);
	helper.addFloatSliderParam("penumbra_angle", 5.0f, 0.0f, 90.0f);

	helper.addBoolParam("is_area", true);
}

void ShaderInfoHelper::buildAreaLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true);

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
	buildCommonLightShaderParams(iri, rendererObjectInfo, true);

	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("spread_angle", 1.0f, 0.0f, 33.0f);
}

void ShaderInfoHelper::buildSkydomeLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)					// for the moment, hopefully we're only going to be connecting Ops
{
	buildCommonLightShaderParams(iri, rendererObjectInfo, true);
}

void ShaderInfoHelper::buildEnvironmentLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("intensity", 1.0f, 0.0f, 5.0f);
	helper.addFloatSliderParam("exposure", 0.0f, 0.0f, 20.0f);
	helper.addStringPopupParam("shadow_type", "normal", shadowTypeOptions, 3);
	helper.addIntParam("num_samples", 1);
	helper.addBoolParam("clamp_luminance", false);

	helper.addStringParam("env_map_path");
}

void ShaderInfoHelper::buildPhysicalSkyLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatParam("intensity", 1.0f);
	helper.addFloatSliderParam("exposure", 0.0f, 0.0f, 20.0f);
	helper.addStringPopupParam("shadow_type", "normal", shadowTypeOptions, 3);
	helper.addIntParam("num_samples", 1);
	helper.addBoolParam("clamp_luminance", false);

	helper.addFloatSliderParam("sky_intensity", 1.0f, 0.0f, 5.0f);
	helper.addFloatSliderParam("sun_intensity", 1.0f, 0.0f, 5.0f);

	helper.addIntParam("hemisphere_extension", 0);

	helper.addFloatSliderParam("turbidity", 3.0f, 0.001f, 20.0f);

	helper.addFloatSliderParam("time", 17.2f, 0.0f, 24.0f);
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

void ShaderInfoHelper::buildConstantTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("colour", Col3f(0.6f, 0.6f, 0.6f));
}

void ShaderInfoHelper::buildCheckerboardTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("scaleU", 1.0f, 0.0001f, 100.0f);
	helper.addFloatSliderParam("scaleV", 1.0f, 0.0001f, 100.0f);

	helper.addColourParam("colour1", Col3f(0.0f, 0.0f, 0.0f));
	helper.addColourParam("colour2", Col3f(1.0f, 1.0f, 1.0f));
}

void ShaderInfoHelper::buildGridTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("scaleU", 1.0f, 0.0001f, 100.0f);
	helper.addFloatSliderParam("scaleV", 1.0f, 0.0001f, 100.0f);

	helper.addColourParam("colour1", Col3f(0.0f, 0.0f, 0.0f));
	helper.addColourParam("colour2", Col3f(1.0f, 1.0f, 1.0f));
}

void ShaderInfoHelper::buildSwatchTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addFloatSliderParam("scaleU", 1.0f, 0.0001f, 100.0f);
	helper.addFloatSliderParam("scaleV", 1.0f, 0.0001f, 100.0f);

	helper.addStringPopupParam("grid_type", "hue", swatchGridTypeOptions, 3);
	helper.addColourParam("grid_colour", Col3f(0.64f, 0.64f, 0.64f));
	helper.addBoolParam("checkerboard", false);
}

void ShaderInfoHelper::buildTextureReadTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addStringParam("texture_path");
}

void ShaderInfoHelper::buildWireframeTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);

	helper.addColourParam("interior_colour", Col3f(0.6f, 0.6f, 0.6f));
	helper.addFloatSliderParam("line_width", 0.005f, 0.0f, 10.0f);

	helper.addColourParam("line_colour", Col3f(0.01f, 0.01f, 0.01f));
	helper.addFloatSliderParam("edge_softness", 0.3f);

	helper.addIntParam("edge_type", 1);
}

//

void ShaderInfoHelper::buildAdjustOpParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);
	
	helper.addFloatSliderParam("adjust_value", 1.0f, 0.0f, 5.0f);
}

void ShaderInfoHelper::buildMixOpParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo)
{
	ShaderInfoHelper helper(iri, rendererObjectInfo);
	
	helper.addFloatSliderParam("mix_value", 0.5f);
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
