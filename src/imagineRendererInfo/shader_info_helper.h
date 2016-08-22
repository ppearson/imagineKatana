#ifndef SHADER_INFO_HELPER_H
#define SHADER_INFO_HELPER_H

#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnGroupBuilder.h>

class ImagineRendererInfo;


class ShaderInfoHelper
{
public:
	ShaderInfoHelper(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);

	struct Col3f
	{
		Col3f(float red, float green, float blue) : r(red), g(green), b(blue)
		{
		}

		float r;
		float g;
		float b;
	};

	static void fillShaderInputNames(const std::string& shaderName, std::vector<std::string>& names);
	static void fillShaderOutputNames(const std::string& shaderName, std::vector<std::string>& names);

	// annoyingly, we have to pass in ImagineRendererInfo in order to actually use stuff in the RenderInfoBase class as lots of stuff is non-static when
	// it should be..
	static bool buildShaderInfo(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
								  const FnKat::GroupAttribute inputAttr);


	// for builders

	static void buildStandardShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildStandardImageShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildGlassShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildMetalShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildPlasticShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildBrushedMetalShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildMetallicPaintShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildTranslucentShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildVelvetShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildLuminousShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);


	static void buildCommonLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo,
											 bool addColour, bool visibleOn);
	static void buildPointLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildSpotLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildAreaLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildDistantLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildSkydomeLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildEnvironmentLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildPhysicalSkyLightShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);

	static void buildBumpTextureShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildAlphaTextureShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);

	// Ops / Textures
	static void buildConstantTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildCheckerboardTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);
	static void buildWireframeTextureParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);


	// for params

	void addFloatParam(const std::string& name, float defaultValue);
	void addFloatSliderParam(const std::string& name, float defaultValue, float sliderMin = 0.0f, float sliderMax = 1.0f);
	void addColourParam(const std::string& name, Col3f defaultValue);
	void addIntParam(const std::string& name, int defaultValue);
	void addIntEnumParam(const std::string& name, int defaultValue, const char** options, unsigned int size);
	void addBoolParam(const std::string& name, bool defaultValue);
	void addStringParam(const std::string& name);
	void addStringPopupParam(const std::string& name, const std::string& defaultValue, const char** options, unsigned int size);

protected:
	const ImagineRendererInfo&				m_iri;
	FnKat::GroupBuilder&					m_rendererObjectInfo;
};

#endif // SHADER_INFO_HELPER_H
