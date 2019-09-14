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

#ifndef IMAGINERENDERERINFO_H
#define IMAGINERENDERERINFO_H

#include <FnRendererInfo/plugin/RendererInfoBase.h>

#include "FnAttribute/FnAttribute.h"
#include "FnAttribute/FnGroupBuilder.h"

class ImagineRendererInfo : public Foundry::Katana::RendererInfo::RendererInfoBase
{
public:
	ImagineRendererInfo();
	virtual ~ImagineRendererInfo();

	static Foundry::Katana::RendererInfo::RendererInfoBase* create();
	static void flush();

	virtual void configureBatchRenderMethod(Foundry::Katana::RendererInfo::DiskRenderMethod& batchRenderMethod) const;

	virtual void fillRenderMethods(std::vector<Foundry::Katana::RendererInfo::RenderMethod*>& renderMethods) const;

	virtual void fillRendererObjectTypes(std::vector<std::string>& renderObjectTypes, const std::string& type) const;

	virtual std::string getRendererObjectDefaultType(const std::string& type) const;

	virtual void fillLiveRenderTerminalOps(OpDefinitionQueue& terminalOps, const FnAttribute::GroupAttribute& stateArgs) const;

	virtual void fillRendererObjectNames(std::vector<std::string>& rendererObjectNames, const std::string& type,
														const std::vector<std::string>& typeTags) const;

	virtual std::string getRegisteredRendererName() const;

	virtual std::string getRegisteredRendererVersion() const;

	virtual bool isPresetLocalFileNeeded(const std::string& outputType) const { return false; }

	virtual bool isNodeTypeSupported(const std::string& nodeType) const;

	virtual void fillShaderInputNames(std::vector<std::string>& shaderInputNames, const std::string& shaderName) const;

	virtual void fillShaderInputTags(std::vector<std::string>& shaderInputTags, const std::string& shaderName,
													const std::string& inputName) const;

	virtual void fillShaderOutputNames(std::vector<std::string>& shaderOutputNames, const std::string& shaderName) const;

	virtual void fillShaderOutputTags(std::vector<std::string>& shaderOutputTags, const std::string& shaderName,
													const std::string& outputName) const;

	virtual void fillRendererShaderTypeTags(std::vector<std::string>& shaderTypeTags, const std::string& shaderType) const;

	virtual bool buildRendererObjectInfo(FnKat::GroupBuilder& rendererObjectInfo, const std::string& name, const std::string& type,
													const FnKat::GroupAttribute inputAttr) const;

	virtual void buildLiveRenderControlModules(FnKat::GroupBuilder& liveRenderControlModules) const;


	// Many functions in the Katana Render API are stupidly not static when they can be and often marked as protected,
	// severely limiting how you can use them and thus organise the code. So provide accessors here that call them from
	// a RendererInfoBase-derived class

	void localAddRenderObjectParam(FnKat::GroupBuilder& renderObjectInfo, const std::string& name, int type, int arraySize,
							  FnKat::Attribute defaultAttr, FnKat::Attribute hintsAttr, const EnumPairVector& enumValues) const;

	void localSetShaderParameterMapping(FnAttribute::GroupBuilder& renderObjectInfo, const std::string& metaName,
								   const std::string& actualName) const;
	
protected:

};


#endif // IMAGINERENDERERINFO_H

