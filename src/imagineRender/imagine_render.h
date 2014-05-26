#ifndef IMAGINERENDER_H
#define IMAGINERENDER_H

#include <Render/RenderBase.h>

namespace FnKat = Foundry::Katana;
namespace FnKatRender = FnKat::Render;

class ImagineRender : public Foundry::Katana::Render::RenderBase
{
public:
	ImagineRender(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute arguments);

	virtual int start();
	virtual int stop();

	virtual void configureDiskRenderOutputProcess(FnKatRender::DiskRenderOutputProcess& diskRender, const std::string& outputName,
												  const std::string& outputPath, const std::string& renderMethodName, const float& frameTime) const;


	static Foundry::Katana::Render::RenderBase* create(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute args)
	{
		return new ImagineRender(rootIterator, args);
	}

protected:
};

#endif // IMAGINERENDER_H
