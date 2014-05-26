
#include "imagine_renderer_info.h"


ImagineRendererInfo::ImagineRendererInfo()
{

}

ImagineRendererInfo::~ImagineRendererInfo()
{

}

void ImagineRendererInfo::fillRenderMethods(std::vector<Foundry::Katana::RendererInfo::RenderMethod*>& renderMethods) const
{
	renderMethods.push_back(new Foundry::Katana::RendererInfo::DiskRenderMethod());
	renderMethods.push_back(new Foundry::Katana::RendererInfo::PreviewRenderMethod());
}

void ImagineRendererInfo::fillRendererObjectTypes(std::vector<std::string>& renderObjectTypes, const std::string& type) const
{
	renderObjectTypes.clear();

	if (type == kFnRendererObjectTypeShader)
	{
		renderObjectTypes.push_back("surface");
		renderObjectTypes.push_back("bump");
		renderObjectTypes.push_back("displacement");
		renderObjectTypes.push_back("alpha");
		renderObjectTypes.push_back("light");
	}
	else if (type == kFnRendererObjectTypeRenderOutput)
	{
		renderObjectTypes.push_back(kFnRendererOutputTypeColor);
		renderObjectTypes.push_back(kFnRendererOutputTypeRaw);
		renderObjectTypes.push_back(kFnRendererOutputTypeDeep);
	}
}

std::string ImagineRendererInfo::getRendererObjectDefaultType(const std::string& type) const
{
	if (type == kFnRendererObjectTypeShader)
	{
		return "surface";
	}
	else if (type == kFnRendererObjectTypeRenderOutput)
	{
		return "color";
	}
}

void ImagineRendererInfo::fillRendererObjectNames(std::vector<std::string>& rendererObjectNames, const std::string& type,
											const std::vector<std::string>& typeTags) const
{
	if (type == kFnRendererObjectTypeShader)
	{
		// TODO: use MaterialFactory to get the full list...
		bool isSurface = std::find(typeTags.begin(), typeTags.end(), "surface") != typeTags.end();

		if (isSurface)
		{
			rendererObjectNames.push_back("Standard");
			rendererObjectNames.push_back("Velvet");
			rendererObjectNames.push_back("Glass");
			rendererObjectNames.push_back("Plastic");
			rendererObjectNames.push_back("Metal");
			rendererObjectNames.push_back("Brushed Metal");
			rendererObjectNames.push_back("Luminous");
			rendererObjectNames.push_back("Metallic Paint");
			rendererObjectNames.push_back("Translucent");
		}
	}
	else if (type == kFnRendererObjectTypeRenderOutput)
	{
		bool isColour = std::find(typeTags.begin(), typeTags.end(), "color") != typeTags.end();
		if (isColour)
		{
			rendererObjectNames.push_back("primary");
			rendererObjectNames.push_back("diffuse");
			rendererObjectNames.push_back("glossy");
			rendererObjectNames.push_back("specular");
			rendererObjectNames.push_back("shadow");
			rendererObjectNames.push_back("z");
			rendererObjectNames.push_back("zn");
			rendererObjectNames.push_back("wpp");
			rendererObjectNames.push_back("n");
			rendererObjectNames.push_back("uv");
		}
	}
}

std::string ImagineRendererInfo::getRegisteredRendererName() const
{
	return "Imagine";
}

std::string ImagineRendererInfo::getRegisteredRendererVersion() const
{
	return "0.92";
}

void ImagineRendererInfo::fillShaderInputNames(std::vector<std::string>& shaderInputNames, const std::string& shaderName) const
{
	if (shaderName == "Standard")
	{

	}
}

void ImagineRendererInfo::fillShaderInputTags(std::vector<std::string>& shaderInputTags, const std::string& shaderName,
										const std::string& inputName) const
{

}

void ImagineRendererInfo::fillShaderOutputNames(std::vector<std::string>& shaderOutputNames, const std::string& shaderName) const
{

}

void ImagineRendererInfo::fillShaderOutputTags(std::vector<std::string>& shaderOutputTags, const std::string& shaderName,
										const std::string& outputName) const
{

}

void ImagineRendererInfo::fillRendererShaderTypeTags(std::vector<std::string>& shaderTypeTags, const std::string& shaderType) const
{

}

bool ImagineRendererInfo::buildRendererObjectInfo(FnKat::GroupBuilder& rendererObjectInfo, const std::string& name, const std::string& type,
												const FnKat::GroupAttribute inputAttr) const
{

}
