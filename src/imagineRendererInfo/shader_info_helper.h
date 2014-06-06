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

	// annoyingly, we have to pass in ImagineRendererInfo in order to actually use stuff in the RenderInfoBase class as lots of stuff is non-static when
	// it should be..
	static bool buildShaderInfo(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
								  const FnKat::GroupAttribute inputAttr);


	// for builders

	static void buildStandardShaderParams(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo);



	// for params

	void addFloatParam(const std::string& name, float defaultValue);
	void addColourParam(const std::string& name, Col3f defaultValue);
	void addBoolParam(const std::string& name, bool defaultValue);

protected:
	const ImagineRendererInfo&				m_iri;
	FnKat::GroupBuilder&					m_rendererObjectInfo;
};

#endif // SHADER_INFO_HELPER_H
