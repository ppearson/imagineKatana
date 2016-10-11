#ifndef IMAGINERENDERERINFO_H
#define IMAGINERENDERERINFO_H

#ifdef KAT_V_2
#include <FnRendererInfo/plugin/RendererInfoBase.h>
#else
#include <RendererInfo/RendererInfoBase.h>
#endif

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
	
#ifdef KAT_V_2
	virtual void fillLiveRenderTerminalOps(OpDefinitionQueue& terminalOps, const FnAttribute::GroupAttribute& stateArgs) const;
#endif

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

protected:

};


#endif // IMAGINERENDERERINFO_H

