#include "render_object_info_helper.h"

#include <vector>

#include <RendererInfo/RendererInfoBase.h>
#include <FnAttribute/FnDataBuilder.h>

#include "imagine_renderer_info.h"

// RenderInfoBase stupidly has these marked as protected, so redefine them here so we can use them easily...
typedef std::pair<std::string, int> EnumPair;
typedef std::vector<EnumPair> EnumPairVector;

RenderObjectInfoHelper::RenderObjectInfoHelper()
{
}

void RenderObjectInfoHelper::configureBasicRenderObjectInfo(FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
															const std::string& type)
{
	std::vector<std::string> typeTags;
	std::string location = name;
	std::string fullPath;

	FnKat::Attribute containerHintsAttribute;

	Foundry::Katana::RendererInfo::RendererInfoBase::configureBasicRenderObjectInfo(rendererObjectInfo, type, typeTags, location, fullPath,
																					kFnRendererObjectValueTypeUnknown, containerHintsAttribute);
}

bool RenderObjectInfoHelper::buildOutputChannel(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name)
{
	configureBasicRenderObjectInfo(rendererObjectInfo, name, kFnRendererObjectTypeOutputChannel);

	FnKat::StringBuilder driverAttributeBuilder;
	driverAttributeBuilder.push_back("driver_exr");
	FnKat::GroupBuilder driverGroupBuilder;
	driverGroupBuilder.set("widget", FnKat::StringAttribute("shaderPopup"));
	driverGroupBuilder.set("dynamicParameters", FnKat::StringAttribute("driverParameters"));
	driverGroupBuilder.set("dynamicParametersType", FnKat::StringAttribute(kFnRendererObjectTypeDriver));
	driverGroupBuilder.set("shaderType", FnKat::StringAttribute(kFnRendererObjectTypeDriver));
	driverGroupBuilder.set("rendererInfo", FnKat::StringAttribute("ImagineRendererInfo"));
	driverGroupBuilder.set("hideProducts", FnKat::StringAttribute("True"));
	//	driverGroupBuilder.set("flat", FnKat::StringAttribute("False"));

	EnumPairVector enums;
	iri.localAddRenderObjectParam(rendererObjectInfo, "driver", kFnRendererObjectValueTypeString, 0,
																		  driverAttributeBuilder.build(), driverGroupBuilder.build(), enums);

	FnKat::StringBuilder channelAttributeBuilder;
	channelAttributeBuilder.push_back("RGBA");
	FnKat::GroupBuilder channelGroupBuilder;

	iri.localAddRenderObjectParam(rendererObjectInfo, "channel", kFnRendererObjectValueTypeString, 0, channelAttributeBuilder.build(),
						 channelGroupBuilder.build(), enums);

	return true;
}

bool RenderObjectInfoHelper::buildRenderOutput(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
											   const FnKat::GroupAttribute inputAttr)
{
	configureBasicRenderObjectInfo(rendererObjectInfo, name, kFnRendererObjectTypeRenderOutput);

	if (name == kFnRendererOutputTypeColor)
	{
		FnKat::StringBuilder outputChannelAttributeBuilder;
		outputChannelAttributeBuilder.push_back("rgba");

		EnumPairVector enums;
		FnKat::StringBuilder channelAttributeBuilder;
		channelAttributeBuilder.push_back("rgba");
		FnKat::GroupBuilder channelGroupBuilder;
		channelGroupBuilder.set("widget", FnKat::StringAttribute("popup"));
		channelGroupBuilder.set("options", outputChannelAttributeBuilder.build());

		iri.localAddRenderObjectParam(rendererObjectInfo, "channel", kFnRendererObjectValueTypeString, 0, channelAttributeBuilder.build(),
							 channelGroupBuilder.build(), enums);
	}

	return true;
}