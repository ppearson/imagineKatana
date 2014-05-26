#ifndef IMAGINERENDERERINFO_H
#define IMAGINERENDERERINFO_H

#include <RendererInfo/RendererInfoBase.h>
#include "FnAttribute/FnAttribute.h"
#include "FnAttribute/FnGroupBuilder.h"

class ImagineRendererInfo : public Foundry::Katana::RendererInfo::RendererInfoBase
{
public:
	ImagineRendererInfo();
	virtual ~ImagineRendererInfo();

	virtual void fillRenderMethods(std::vector<Foundry::Katana::RendererInfo::RenderMethod*>& renderMethods) const;

	virtual void fillRendererObjectTypes(std::vector<std::string>& renderObjectTypes, const std::string& type) const;

	virtual std::string getRendererObjectDefaultType(const std::string& type) const;

	virtual void fillRendererObjectNames(std::vector<std::string>& rendererObjectNames, const std::string& type,
														const std::vector<std::string>& typeTags) const;

	virtual std::string getRegisteredRendererName() const;

	virtual std::string getRegisteredRendererVersion() const;

	virtual bool isPresetLocalFileNeeded(const std::string& outputType) const { return false; }

	virtual bool isNodeTypeSupported(const std::string& nodeType) const { return false; }

	virtual bool isPolymeshFacesetSplittingEnabled() const { return true; }

	virtual void fillShaderInputNames(std::vector<std::string>& shaderInputNames, const std::string& shaderName) const;

	virtual void fillShaderInputTags(std::vector<std::string>& shaderInputTags, const std::string& shaderName,
													const std::string& inputName) const;

	virtual void fillShaderOutputNames(std::vector<std::string>& shaderOutputNames, const std::string& shaderName) const;

	virtual void fillShaderOutputTags(std::vector<std::string>& shaderOutputTags, const std::string& shaderName,
													const std::string& outputName) const;

	virtual void fillRendererShaderTypeTags(std::vector<std::string>& shaderTypeTags, const std::string& shaderType) const;

	virtual bool buildRendererObjectInfo(FnKat::GroupBuilder& rendererObjectInfo, const std::string& name, const std::string& type,
													const FnKat::GroupAttribute inputAttr = NULL) const;

protected:

};


#endif // IMAGINERENDERERINFO_H

