/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

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
								  const FnKat::GroupAttribute& inputAttr);

};

#endif // RENDER_OBJECT_INFO_HELPER_H
