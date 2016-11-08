#ifndef RENDER_OBJECT_INFO_HELPER_H
#define RENDER_OBJECT_INFO_HELPER_H

#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnGroupBuilder.h>

class ImagineRendererInfo;

class RenderObjectInfoHelper
{
public:
	RenderObjectInfoHelper();

	// annoyingly, we have to pass in ImagineRendererInfo in order to actually use stuff in the RenderInfoBase class as lots of stuff is non-static when
	// it should/could be...
	static void configureBasicRenderObjectInfo(FnKat::GroupBuilder& rendererObjectInfo, const std::string& name, const std::string& type);
	static bool buildOutputChannel(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name);

	static bool buildRenderOutput(const ImagineRendererInfo& iri, FnKat::GroupBuilder& rendererObjectInfo, const std::string& name,
								  const FnKat::GroupAttribute inputAttr);

};

#endif // RENDER_OBJECT_INFO_HELPER_H
