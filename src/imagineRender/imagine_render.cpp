#include "imagine_render.h"

#include <stdio.h>

#include <Render/GlobalSettings.h>
#include <RendererInfo/RenderMethod.h>

#include "sg_location_processor.h"

ImagineRender::ImagineRender(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute arguments) :
	RenderBase(rootIterator, arguments)
{
}

int ImagineRender::start()
{
	FnKat::FnScenegraphIterator rootIterator = getRootIterator();

	std::string renderMethodName = getRenderMethodName();

	// Batch renders are just disk renders really...
	if (renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kBatchName)
	{
		renderMethodName = FnKat::RendererInfo::DiskRenderMethod::kDefaultName;
	}

	float renderFrame = getRenderTime();

	FnKatRender::RenderSettings renderSettings(rootIterator);
	FnKatRender::GlobalSettings globalSettings(rootIterator, "imagine");

	if (!configureGeneralSettings(renderSettings, rootIterator))
	{
		fprintf(stderr, "Can't configure general settings...\n");
		return -1;
	}

	if (renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kDefaultName)
	{
		if (!configureRenderOutputs(renderSettings, rootIterator))
		{
			fprintf(stderr, "Can't find any valid Render Outputs for Disk Render...\n");
			return -1;
		}

		performDiskRender(renderSettings, rootIterator);

		return 0;
	}

	return -1;
}

int ImagineRender::stop()
{
	return 0;
}

void ImagineRender::configureDiskRenderOutputProcess(FnKatRender::DiskRenderOutputProcess& diskRender, const std::string& outputName,
											  const std::string& outputPath, const std::string& renderMethodName, const float& frameTime) const
{
	// get the render settings
	FnKat::FnScenegraphIterator rootIterator = getRootIterator();
	FnKatRender::RenderSettings renderSettings(rootIterator);

	// generate paths for temporary and final render target files...
	// Katana gives us the path to the temporary render file and we render to that.
	// Katana will then copy this with exrcopyattributes to the final target location path defined via
	// RenderOutputDefine after we finish the render

	std::string tempRenderPath = FnKat::RenderOutputUtils::buildTempRenderLocation(rootIterator, outputName, "render", "exr", frameTime);
	std::string finalTargetPath = outputPath;

	// Get the attributes we need for the render output from renderSettings attrib on the root node.
	FnKatRender::RenderSettings::RenderOutput output = renderSettings.getRenderOutputByName(outputName);

	std::auto_ptr<FnKatRender::RenderAction> renderAction;

	if (output.type == "color")
	{
		if (renderSettings.isTileRender())
		{
			renderAction.reset(new FnKatRender::CopyRenderAction(finalTargetPath, tempRenderPath));
		}
		else
		{
			renderAction.reset(new FnKatRender::CopyAndConvertRenderAction(finalTargetPath, tempRenderPath, output.clampOutput,
																			 output.colorConvert, output.computeStats, output.convertSettings));
		}
	}

	diskRender.setRenderAction(renderAction);
}

bool ImagineRender::configureGeneralSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{

	unsigned int renderWidth = settings.getResolutionX();
	unsigned int renderHeight = settings.getResolutionY();

	fprintf(stderr, "Render dimensions: %d, %d\n", renderWidth, renderHeight);

	return true;
}

bool ImagineRender::configureRenderOutputs(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	// for the moment, just use the first "color" type as the main primary AOV
	FnKatRender::RenderSettings::RenderOutputs outputs = settings.getRenderOutputs();

	if (outputs.empty())
		return false;

	std::vector<std::string> renderOutputNames = settings.getRenderOutputNames();
	std::vector<std::string>::const_iterator it = renderOutputNames.begin();

	for (; it != renderOutputNames.end(); ++it)
	{
		const FnKatRender::RenderSettings::RenderOutput& renderOutput = outputs[(*it)];

		if (renderOutput.type == "color")
		{
			m_diskRenderOutputPath = renderOutput.renderLocation;
			break;
		}
	}

	return true;
}

void ImagineRender::buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	// force expand for the moment, instead of using lazy procedurals...
	SGLocationProcessor::processSGForceExpand(rootIterator);
}

void ImagineRender::performDiskRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	if (m_diskRenderOutputPath.empty())
		return;

	fprintf(stderr, "Performing disk render to: %s\n", m_diskRenderOutputPath.c_str());
}

DEFINE_RENDER_PLUGIN(ImagineRender)

void registerPlugins()
{
	REGISTER_PLUGIN(ImagineRender, "imagine", 0, 1);
}
