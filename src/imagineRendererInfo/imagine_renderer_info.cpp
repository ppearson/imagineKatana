
#include "imagine_renderer_info.h"

#include <algorithm>

#include <stdio.h>

#include "render_object_info_helper.h"
#include "shader_info_helper.h"

ImagineRendererInfo::ImagineRendererInfo()
{

}

ImagineRendererInfo::~ImagineRendererInfo()
{

}

Foundry::Katana::RendererInfo::RendererInfoBase* ImagineRendererInfo::create()
{
	return new ImagineRendererInfo();
}

void ImagineRendererInfo::flush()
{

}

void ImagineRendererInfo::configureBatchRenderMethod(Foundry::Katana::RendererInfo::DiskRenderMethod& batchRenderMethod) const
{

}

void ImagineRendererInfo::fillRenderMethods(std::vector<Foundry::Katana::RendererInfo::RenderMethod*>& renderMethods) const
{
	renderMethods.push_back(new Foundry::Katana::RendererInfo::DiskRenderMethod());
	renderMethods.push_back(new Foundry::Katana::RendererInfo::PreviewRenderMethod());

	renderMethods.push_back(new Foundry::Katana::RendererInfo::LiveRenderMethod());
}

void ImagineRendererInfo::fillRendererObjectTypes(std::vector<std::string>& renderObjectTypes, const std::string& type) const
{
	renderObjectTypes.clear();

	if (type == kFnRendererObjectTypeShader)
	{
		renderObjectTypes.push_back("surface");
		renderObjectTypes.push_back("bump");
		renderObjectTypes.push_back("medium");
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
/*	else if (type == kFnRendererObjectTypeOutputChannel)
	{
		renderObjectTypes.push_back("primary");
		renderObjectTypes.push_back("rgba");
	}
*/
}

std::string ImagineRendererInfo::getRendererObjectDefaultType(const std::string& type) const
{
	if (type == kFnRendererObjectTypeShader)
	{
		return "surface";
	}
	else if (type == kFnRendererObjectTypeRenderOutput)
	{
		return kFnRendererOutputTypeColor;
	}

	return "";
}

#ifdef KAT_V_2
void ImagineRendererInfo::fillLiveRenderTerminalOps(OpDefinitionQueue& terminalOps, const FnAttribute::GroupAttribute& stateArgs) const
{
	{
        FnAttribute::GroupBuilder opArgs;
        terminalOps.push_back(std::make_pair("LiveAttribute", opArgs.build()));
    }
	
	{
        // Watch for changes any locations that are cameras
        FnAttribute::GroupBuilder opArgs;
        opArgs.set("type", FnAttribute::StringAttribute("camera"));
        opArgs.set("location", FnAttribute::StringAttribute("/root/world"));

        std::string attributes[] = {"xform", "geometry"};
        opArgs.set("attributeNames", FnAttribute::StringAttribute(attributes, 2, 1));
        terminalOps.push_back(std::make_pair("LiveRenderFilter", opArgs.build()));
    }
	
	{
        // Watch for changes any locations that are lights
        FnAttribute::GroupBuilder opArgs;
        opArgs.set("type", FnAttribute::StringAttribute("light"));
        opArgs.set("location", FnAttribute::StringAttribute("/root/world"));
        opArgs.set("collectUpstream", FnAttribute::IntAttribute(1));

        std::string attributes[] = {"xform", "material", "mute", "solo"};
        opArgs.set("attributeNames", FnAttribute::StringAttribute(attributes, 4, 1));
        terminalOps.push_back(std::make_pair("LiveRenderFilter", opArgs.build()));
    }
	
	{
        // Watch for changes to material attributes on any locations that are not lights
        FnAttribute::GroupBuilder opArgs;
        opArgs.set("type", FnAttribute::StringAttribute(""));
        opArgs.set("location", FnAttribute::StringAttribute("/root"));
        opArgs.set("typeAlias", FnAttribute::StringAttribute("geoMaterial"));
        opArgs.set("exclusions", FnAttribute::StringAttribute("light"));
        opArgs.set("collectUpstream", FnAttribute::IntAttribute(1));

        std::string attributes[] = {"info", "material", "geometry.faces",};
        opArgs.set("attributeNames", FnAttribute::StringAttribute(attributes, 3, 1));
        terminalOps.push_back(std::make_pair("LiveRenderFilter", opArgs.build()));
    }
}
#endif

void ImagineRendererInfo::fillRendererObjectNames(std::vector<std::string>& rendererObjectNames, const std::string& type,
											const std::vector<std::string>& typeTags) const
{
	if (type == kFnRendererObjectTypeShader)
	{
		// TODO: use MaterialFactory to get the full list...
		bool isSurface = std::find(typeTags.begin(), typeTags.end(), "surface") != typeTags.end();
		bool isLight = std::find(typeTags.begin(), typeTags.end(), "light") != typeTags.end();
		bool isBump = std::find(typeTags.begin(), typeTags.end(), "bump") != typeTags.end();
		bool isAlpha = std::find(typeTags.begin(), typeTags.end(), "alpha") != typeTags.end();

		if (isSurface)
		{
			rendererObjectNames.push_back("Standard");
			rendererObjectNames.push_back("StandardImage");
			rendererObjectNames.push_back("Velvet");
			rendererObjectNames.push_back("Glass");
			rendererObjectNames.push_back("Plastic");
			rendererObjectNames.push_back("Metal");
			rendererObjectNames.push_back("Brushed Metal");
			rendererObjectNames.push_back("Luminous");
			rendererObjectNames.push_back("Metallic Paint");
			rendererObjectNames.push_back("Translucent");
		}
		else if (isLight)
		{
			rendererObjectNames.push_back("Point");
			rendererObjectNames.push_back("Spot");
			rendererObjectNames.push_back("Area");
			rendererObjectNames.push_back("Distant");
			rendererObjectNames.push_back("SkyDome");
			rendererObjectNames.push_back("Environment");
			rendererObjectNames.push_back("PhysicalSky");
		}
		else if (isBump)
		{
			rendererObjectNames.push_back("ImageTextureBump");
		}
		else if (isAlpha)
		{
			rendererObjectNames.push_back("ImageTextureAlpha");
		}
		else
		{
			// if we weren't given any tags, then we need to add items that should be network material nodes.

			// annoyingly, Katana doesn't differentiate these by type, so the user just gets presented with
			// a flat list. So prefix the type within the name itself to sort them to some degree.

			rendererObjectNames.push_back("Shader/Standard");
			rendererObjectNames.push_back("Shader/StandardImage");
			rendererObjectNames.push_back("Shader/Glass");
			rendererObjectNames.push_back("Shader/Metal");
			rendererObjectNames.push_back("Shader/Plastic");
			rendererObjectNames.push_back("Shader/MetallicPaint");
			rendererObjectNames.push_back("Shader/Translucent");

			rendererObjectNames.push_back("Op/Adjust");
			rendererObjectNames.push_back("Op/Mix");

			rendererObjectNames.push_back("Texture/Checkerboard");
			rendererObjectNames.push_back("Texture/Constant");
			rendererObjectNames.push_back("Texture/Grid");
			rendererObjectNames.push_back("Texture/Swatch");
			rendererObjectNames.push_back("Texture/TextureRead");
			rendererObjectNames.push_back("Texture/Wireframe");
		}
	}
	else if (type == kFnRendererObjectTypeRenderOutput)
	{
		bool isColour = std::find(typeTags.begin(), typeTags.end(), "color") != typeTags.end();
		if (isColour)
		{
			rendererObjectNames.push_back("primary");
			rendererObjectNames.push_back("diffuse");
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
	return "imagine";
}

std::string ImagineRendererInfo::getRegisteredRendererVersion() const
{
	return "0.99";
}

bool ImagineRendererInfo::isNodeTypeSupported(const std::string& nodeType) const
{
	if (nodeType == "OutputChannelDefine" || nodeType == "ShadingNode")
		return true;

	return false;
}

void ImagineRendererInfo::fillShaderInputNames(std::vector<std::string>& shaderInputNames, const std::string& shaderName) const
{
	ShaderInfoHelper::fillShaderInputNames(shaderName, shaderInputNames);
}

void ImagineRendererInfo::fillShaderInputTags(std::vector<std::string>& shaderInputTags, const std::string& shaderName,
										const std::string& inputName) const
{

}

void ImagineRendererInfo::fillShaderOutputNames(std::vector<std::string>& shaderOutputNames, const std::string& shaderName) const
{
	ShaderInfoHelper::fillShaderOutputNames(shaderName, shaderOutputNames);
}

void ImagineRendererInfo::fillShaderOutputTags(std::vector<std::string>& shaderOutputTags, const std::string& shaderName,
										const std::string& outputName) const
{

}

void ImagineRendererInfo::fillRendererShaderTypeTags(std::vector<std::string>& shaderTypeTags, const std::string& shaderType) const
{
	if (shaderType == "surface")
	{
		shaderTypeTags.push_back("float or rgb");
	}
}

bool ImagineRendererInfo::buildRendererObjectInfo(FnKat::GroupBuilder& rendererObjectInfo, const std::string& name, const std::string& type,
												const FnKat::GroupAttribute inputAttr) const
{
	if (type == kFnRendererObjectTypeOutputChannel)
	{
		return RenderObjectInfoHelper::buildOutputChannel(*this, rendererObjectInfo, name);
	}

	if (type == kFnRendererObjectTypeRenderOutput)
	{
		return RenderObjectInfoHelper::buildRenderOutput(*this, rendererObjectInfo, name, inputAttr);
	}

	if (type == kFnRendererObjectTypeShader)
	{
		return ShaderInfoHelper::buildShaderInfo(*this, rendererObjectInfo, name, inputAttr);
	}

	return false;
}

void ImagineRendererInfo::buildLiveRenderControlModules(FnKat::GroupBuilder& liveRenderControlModules) const
{

}

void ImagineRendererInfo::localAddRenderObjectParam(FnKat::GroupBuilder& renderObjectInfo, const std::string& name, int type, int arraySize,
						  FnKat::Attribute defaultAttr, FnKat::Attribute hintsAttr, const EnumPairVector& enumValues) const
{
	addRenderObjectParam(renderObjectInfo, name, type, arraySize, defaultAttr, hintsAttr, enumValues);
}

namespace
{
	DEFINE_RENDERERINFO_PLUGIN(ImagineRendererInfo)
}

void registerPlugins()
{
	REGISTER_PLUGIN(ImagineRendererInfo, "ImagineRendererInfo", 0, 1);
}
