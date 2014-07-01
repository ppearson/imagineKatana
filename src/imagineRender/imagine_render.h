#ifndef IMAGINE_RENDER_H
#define IMAGINE_RENDER_H

#include <Render/RenderBase.h>

#define ENABLE_PREVIEW_RENDERS 0

#if ENABLE_PREVIEW_RENDERS
#include <FnDisplayDriver/FnKatanaDisplayDriver.h>
#endif

namespace FnKat = Foundry::Katana;
namespace FnKatRender = FnKat::Render;

#ifndef STAND_ALONE

#include "scene.h"
#include "utils/params.h"
#include "raytracer/raytracer_common.h"

#endif

class ImagineRender : public Foundry::Katana::Render::RenderBase, RaytracerHost
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

	static void flush()
	{

	}


	// progress back from renderer

	virtual void progressChanged(float progress);

	virtual void tileDone(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int threadID);

protected:

	bool configureGeneralSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);
	bool configureRenderSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);
	bool configureRenderOutputs(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void buildCamera(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator cameraIterator);
	void buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void performDiskRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);
	void performPreviewRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void startRenderer();
	void renderFinished();

protected:

#ifndef STAND_ALONE
	Scene*					m_pScene;
	Params					m_renderSettings;

#endif

	std::string				m_diskRenderOutputPath;

	//
	std::string				m_katanaHost;
	unsigned long			m_katanaPort;

#if ENABLE_PREVIEW_RENDERS
	FnKat::KatanaPipe*		m_pDataPipe;
	FnKat::NewChannelMessage* m_pChannel;
#endif

	//

	bool					m_useCompactGeometry;
	bool					m_deduplicateVertexNormals;
	bool					m_printStatistics;

	unsigned int			m_renderWidth;
	unsigned int			m_renderHeight;

	int						m_renderThreads;

	int						m_lastProgress;
};

#endif // IMAGINE_RENDER_H
