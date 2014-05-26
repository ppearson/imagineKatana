#include "imaginerender.h"

#include <Render/GlobalSettings.h>

ImagineRender::ImagineRender(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute arguments) :
	RenderBase(rootIterator, arguments)
{
}

int ImagineRender::start()
{
	FnKat::FnScenegraphIterator rootIterator = getRootIterator();

	std::string renderMethod = getRenderMethodName();

	float renderFrame = getRenderTime();

	FnKatRender::RenderSettings renderSettings(rootIterator);
	FnKatRender::GlobalSettings globalSettings(rootIterator, "imagine");

	if (renderMethod == "disk")
	{

	}
}

int ImagineRender::stop()
{

}

void ImagineRender::configureDiskRenderOutputProcess(FnKatRender::DiskRenderOutputProcess& diskRender, const std::string& outputName,
											  const std::string& outputPath, const std::string& renderMethodName, const float& frameTime) const
{

}
