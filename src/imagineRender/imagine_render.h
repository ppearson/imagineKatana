#ifndef IMAGINE_RENDER_H
#define IMAGINE_RENDER_H

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

	static void flush()
	{

	}

protected:

	bool configureGeneralSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);
	bool configureRenderOutputs(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);

	void performDiskRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator);



protected:

	std::string			m_diskRenderOutputPath;
};

#endif // IMAGINE_RENDER_H
