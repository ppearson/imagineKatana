#include "imagine_render.h"

#include <stdio.h>

#ifdef KAT_V_2
#include <FnRender/plugin/GlobalSettings.h>
#include <FnRendererInfo/plugin/RenderMethod.h>
#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#else
#include <Render/GlobalSettings.h>
#include <RendererInfo/RenderMethod.h>
#include <RenderOutputUtils/RenderOutputUtils.h>
#endif

// include any Imagine headers directly from the source directory as Imagine hasn't got an API yet...
#include "objects/camera.h"
#include "image/output_image.h"
#include "io/image/image_writer_exr.h"
#include "image/image_cache.h"

#include "lights/physical_sky.h"

#include "utils/file_helpers.h"
#include "utils/memory.h"
#include "utils/string_helpers.h"
#include "utils/timer.h"
#include "utils/system.h"

#include "global_context.h"
#include "output_context.h"

#include "utilities.h"

#include "katana_helpers.h"
#include "sg_location_processor.h"

using namespace Imagine;

ImagineRender::ImagineRender(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute arguments) :
	RenderBase(rootIterator, arguments), m_pScene(NULL), m_printMemoryStatistics(0), m_integratorType(1),
	m_ambientOcclusion(false), m_fastLiveRenders(false), m_motionBlur(false),
	m_ROIActive(false)
{
#if ENABLE_PREVIEW_RENDERS
	m_pOutputImage = NULL;
	m_pDataPipe = NULL;
	m_pFrame = NULL;
	m_pPrimaryChannel = NULL;
	
	m_enableIDPicking = true;
#endif

	m_pRaytracer = NULL;

	m_renderThreads = System::getNumberOfThreads() - 1;

	m_renderWidth = 512;
	m_renderHeight = 512;
}

int ImagineRender::start()
{
	FnKat::FnScenegraphIterator rootIterator = getRootIterator();
	FnKat::GroupAttribute imagineGSAttribute = rootIterator.getAttribute("imagineGlobalStatements");
	if (imagineGSAttribute.isValid())
	{
		FnKat::IntAttribute debugPopupAttribute = imagineGSAttribute.getChildByName("debug_popup");
		if (debugPopupAttribute.isValid() && debugPopupAttribute.getValue(0, false) == 1)
		{
			int ret = system("xmessage debug\n");
		}
		
		FnKat::IntAttribute enableIDPickingAttribute = imagineGSAttribute.getChildByName("enable_id_picking");
		if (enableIDPickingAttribute.isValid())
		{
			m_enableIDPicking = (enableIDPickingAttribute.getValue(1, false) == 1);
		}
	}

	std::string renderMethodName = getRenderMethodName();

	// Batch renders are just disk renders really...
	if (renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kBatchName)
	{
		renderMethodName = FnKat::RendererInfo::DiskRenderMethod::kDefaultName;
	}
	
	// Katana automatically turns this off for Disk renders, so we don't have to do that...
	m_enableIDPicking &= useRenderPassID();

	m_lastProgress = 0;
	m_extraAOVsFlags = 0;

	Utilities::registerFileReaders();

	float renderFrame = getRenderTime();
	OutputContext::instance().setFrame(renderFrame);

	// create the scene - this is going to leak for the moment...
	m_pScene = new Scene(true); // don't want a GUI with OpenGL...

	// we want to do things like bbox calculation, vertex normal calculation (for Subdiv approximations) and triangle tessellation
	// in parallel across multiple threads
	m_pScene->setParallelGeoBuild(true);

	// set lazy mode
	m_pScene->setLazy(true);

	FnKatRender::RenderSettings renderSettings(rootIterator);
	FnKatRender::GlobalSettings globalSettings(rootIterator, "imagine");

	bool diskRender = renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kDefaultName;

	if (!configureGeneralSettings(renderSettings, rootIterator, diskRender))
	{
		fprintf(stderr, "Can't configure general settings...\n");
		return -1;
	}

	if (renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kDefaultName)
	{
		if (!configureDiskRenderOutputs(renderSettings, rootIterator))
		{
			fprintf(stderr, "Error: Can't find any valid Render Outputs for Disk Render...\n");
			return -1;
		}

		performDiskRender(renderSettings, rootIterator);

		return 0;
	}
	else if (renderMethodName == FnKat::RendererInfo::PreviewRenderMethod::kDefaultName)
	{
		performPreviewRender(renderSettings, rootIterator);

		return 0;
	}
	else if (renderMethodName == FnKat::RendererInfo::LiveRenderMethod::kDefaultName)
	{
		performLiveRender(renderSettings, rootIterator);

		return 0;
	}

	return -1;
}

int ImagineRender::stop()
{
	// Katana doesn't call this currently...
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

bool ImagineRender::configureGeneralSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator, bool diskRender)
{
	m_renderWidth = settings.getResolutionX();
	m_renderHeight = settings.getResolutionY();

//	fprintf(stderr, "Render dimensions: %d, %d\n", m_renderWidth, m_renderHeight);

	int dataWindow[4];
	settings.getDataWindow(dataWindow);

//	fprintf(stderr, "Data window: (%i, %i, %i, %i)\n", dataWindow[0], dataWindow[1], dataWindow[2], dataWindow[3]);

	m_renderSettings.add("width", m_renderWidth);
	m_renderSettings.add("height", m_renderHeight);

	// work out number of threads to use for rendering
	settings.applyRenderThreads(m_renderThreads);
	applyRenderThreadsOverride(m_renderThreads);

	Foundry::Katana::StringAttribute cameraNameAttribute = rootIterator.getAttribute("renderSettings.cameraName");
	m_renderCameraLocation = cameraNameAttribute.getValue("/root/world/cam/camera", false);

	FnKat::FnScenegraphIterator cameraIterator = rootIterator.getByPath(m_renderCameraLocation);
	if (!cameraIterator.isValid())
	{
		fprintf(stderr, "Error: Can't get hold of render camera attributes...\n");
		return false;
	}

	FnKat::GroupAttribute renderSettingsAttribute = rootIterator.getAttribute("renderSettings");

	FnKat::IntAttribute regionOfInterestAttribute = renderSettingsAttribute.getChildByName("ROI");
	if (regionOfInterestAttribute.isValid())
	{
		FnKat::IntConstVector roiValues = regionOfInterestAttribute.getNearestSample(0.0f);

		// should be four of them...
//		fprintf(stderr, "ROI found: (%i, %i, %i, %i)\n", roiValues[0], roiValues[1], roiValues[2], roiValues[3]);

		m_ROIActive = true;

		// adjust how big our render backbuffer area is
		m_ROIWidth = roiValues[2];
		m_ROIHeight = roiValues[3];

		m_ROIStartX = roiValues[0];
		// invert this
		m_ROIStartY = m_renderHeight - roiValues[1];
		// offset it by height of area, as Imagine's Y origin is opposite Katana's
		m_ROIStartY -= m_ROIHeight;

		m_renderSettings.add("renderCrop", true);
		m_renderSettings.add("cropX", m_ROIStartX);
		m_renderSettings.add("cropY", m_ROIStartY);
		m_renderSettings.add("cropWidth", roiValues[2]);
		m_renderSettings.add("cropHeight", roiValues[3]);
	}
	else
	{
		m_ROIStartX = 0;
		m_ROIStartY = 0;
		m_ROIWidth = m_renderWidth;
		m_ROIHeight = m_renderHeight;
	}

	configureRenderSettings(settings, rootIterator, diskRender);

	// build camera at the end (after motion blur settings have been loaded by above)
	buildCamera(settings, cameraIterator);

	return true;
}

bool ImagineRender::configureDiskRenderOutputs(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	// Katana calls this function from the main thread before renderboot starts (and calls it three times for some reason...)
	
	// for the moment, just use the first "color" type as the main primary AOV
	FnKatRender::RenderSettings::RenderOutputs outputs = settings.getRenderOutputs();

	if (outputs.empty())
		return false;

	// TODO: this assumes that all outputs are to the same file for the moment, which won't necessarily be the case,
	//		 so this only works with merged RenderOutputDefines...

	std::vector<std::string> renderOutputNames = settings.getRenderOutputNames();
	std::vector<std::string>::const_iterator it = renderOutputNames.begin();

	for (; it != renderOutputNames.end(); ++it)
	{
		const std::string& outputName = *it;
		const FnKatRender::RenderSettings::RenderOutput& renderOutput = outputs[outputName];

		if (renderOutput.type == "color" && outputName == "primary")
		{
			// main primary / beauty pass
			m_diskRenderOutputPath = renderOutput.renderLocation;

			m_diskRenderConvertFromLinear = renderOutput.colorConvert;
		}
		else if (outputName == "z")
		{
			m_renderSettings.add("output_realz", true);
			m_extraAOVsFlags |= COMPONENT_DEPTH_REAL;
		}
		else if (outputName == "zn")
		{
			m_renderSettings.add("output_z", true);
			m_extraAOVsFlags |= COMPONENT_DEPTH_NORMALISED;
		}
		else if (outputName == "wpp")
		{
			m_renderSettings.add("output_wpp", true);
			m_extraAOVsFlags |= COMPONENT_WPP;
		}
		else if (outputName == "n")
		{
			m_renderSettings.add("output_normals", true);
			m_extraAOVsFlags |= COMPONENT_NORMAL;
		}
		else if (outputName == "shadow")
		{
			m_renderSettings.add("output_shadows", true);
			m_extraAOVsFlags |= COMPONENT_SHADOWS;
		}
	}

	return true;
}

void ImagineRender::buildCamera(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator cameraIterator)
{
	FnKat::GroupAttribute cameraGeometryAttribute = cameraIterator.getAttribute("geometry");
	if (!cameraGeometryAttribute.isValid())
	{
		fprintf(stderr, "Error: Can't find geometry attribute on the camera: %s...\n", cameraIterator.getFullName().c_str());
		return;
	}

	// screen window - for the moment, it's just the render res, with no overscan or crop....
	FnKat::DoubleAttribute leftAttrib = cameraGeometryAttribute.getChildByName("left");
	FnKat::DoubleAttribute rightAttrib = cameraGeometryAttribute.getChildByName("right");
	FnKat::DoubleAttribute topAttrib = cameraGeometryAttribute.getChildByName("top");
	FnKat::DoubleAttribute bottomAttrib = cameraGeometryAttribute.getChildByName("bottom");

	FnKat::DoubleAttribute fovAttribute = cameraGeometryAttribute.getChildByName("fov");

	float fovValue = fovAttribute.getValue(45.0f, false);

	// TODO: other camera attribs

	// get Camera transform matrix
	FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(cameraIterator);
	const double* pMatrix = xforms[0].getValues();

//	fprintf(stderr, "Camera Matrix:\n%f, %f, %f, %f,\n%f, %f, %f, %f,\n%f, %f, %f, %f,\n%f, %f, %f, %f\n", pMatrix[0], pMatrix[1], pMatrix[2], pMatrix[3],
//			pMatrix[4], pMatrix[5], pMatrix[6], pMatrix[7], pMatrix[8], pMatrix[9], pMatrix[10], pMatrix[11], pMatrix[12], pMatrix[13],
//			pMatrix[14], pMatrix[15]);

	Camera* pRenderCamera = new Camera();
	pRenderCamera->setFOV(fovValue);
	pRenderCamera->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	FnKat::FnScenegraphIterator itRoot = cameraIterator.getRoot();

	// try and find these camera attributes first on the camera, if not look for backup global settings
	FnKat::FloatAttribute focusDistanceAttribute = cameraGeometryAttribute.getChildByName("focus_distance");
	if (!focusDistanceAttribute.isValid() && itRoot.isValid())
	{
		// try and find it next in the global settings
		focusDistanceAttribute = itRoot.getAttribute("imagineGlobalStatements.cam_focal_distance");
	}
	if (focusDistanceAttribute.isValid())
	{
		float focusDistance = focusDistanceAttribute.getValue(16.0f, false);
		// Imagine currently needs negative value...
		pRenderCamera->setFocusDistance(-focusDistance);
	}

	FnKat::FloatAttribute apertureSizeAttribute = cameraGeometryAttribute.getChildByName("aperture_size");
	if (!apertureSizeAttribute.isValid() && itRoot.isValid())
	{
		// try and find it next in the global settings
		apertureSizeAttribute = itRoot.getAttribute("imagineGlobalStatements.cam_aperture_size");
	}
	if (apertureSizeAttribute.isValid())
	{
		float apertureSize = apertureSizeAttribute.getValue(0.0f, false);
		pRenderCamera->setApertureRadius(apertureSize);
	}

	bool detectCameraType = true;
	Camera::ProjectionType cameraProjectionType = Camera::ePerspective;

	if (itRoot.isValid())
	{
		FnKat::IntAttribute cameraProjectionOverrideAttr = itRoot.getAttribute("imagineGlobalStatements.cam_projection_type");
		if (cameraProjectionOverrideAttr.isValid())
		{
			int cameraProjectionOverrideType = cameraProjectionOverrideAttr.getValue(0, false);
			if (cameraProjectionOverrideType == 2)
			{
				cameraProjectionType = Camera::eSpherical;
				detectCameraType = false;
			}
			else if (cameraProjectionOverrideType == 3)
			{
				cameraProjectionType = Camera::eOrthographic;
				detectCameraType = false;
			}
			else if (cameraProjectionOverrideType == 4)
			{
				cameraProjectionType = Camera::eFisheye;
				detectCameraType = false;
			}
		}

		FnKat::IntAttribute environmentVisibleAttr = itRoot.getAttribute("imagineGlobalStatements.environment_visible");
		if (environmentVisibleAttr.isValid())
		{
			int environmentVisible = environmentVisibleAttr.getValue(0, false);
			if (environmentVisible == 1)
			{
				m_pScene->getProjectProperties().setBackgroundType(eBackgroundEnvironment);
			}
		}
		
		FnKat::IntAttribute camUseNearClipAttr = itRoot.getAttribute("imagineGlobalStatements.cam_use_near_clip");
		if (camUseNearClipAttr.isValid())
		{
			int camUseNearClip = camUseNearClipAttr.getValue(0, false);
			if (camUseNearClip == 1)
			{
				float cameraClipping[2];
				settings.getCameraSettings()->getClipping(cameraClipping);
				
				pRenderCamera->setNearClippingPlane(cameraClipping[0]);
			}
		}
	}

	if (detectCameraType && cameraGeometryAttribute.isValid())
	{
		FnKat::StringAttribute cameraProjectionAttribute = cameraGeometryAttribute.getChildByName("projection");
		std::string cameraProjectionValue = cameraProjectionAttribute.getValue("perspective", false);

		if (cameraProjectionValue == "spherical")
		{
			cameraProjectionType = Camera::eSpherical;
		}
		else if (cameraProjectionValue == "orthographic")
		{
			cameraProjectionType = Camera::eOrthographic;
		}
		else if (cameraProjectionValue == "fisheye")
		{
			cameraProjectionType = Camera::eFisheye;
		}
	}

	pRenderCamera->setProjectionType(cameraProjectionType);

	// get some things from renderSettings
	m_creationSettings.m_shutterOpen = settings.getShutterOpen();
	m_creationSettings.m_shutterClose = settings.getShutterClose();

	if (m_creationSettings.m_motionBlur)
	{
		float clampedShutterOpen = m_creationSettings.m_shutterOpen;
		float clampedShutterClose = m_creationSettings.m_shutterClose;
		clampedShutterOpen = clamp(clampedShutterOpen, 0.0f, clampedShutterOpen);
		clampedShutterClose = clamp(clampedShutterClose, clampedShutterClose, 1.0f);
		pRenderCamera->setRelativeShutterTimes(clampedShutterOpen, clampedShutterClose);
	}
	
	// now get any arbitrary attributes
	FnKat::GroupAttribute projectionArbitraryAttribute = cameraIterator.getAttribute("projection.arbitrary");
	if (projectionArbitraryAttribute.isValid())
	{
		unsigned int numChildren = projectionArbitraryAttribute.getNumberOfChildren();
		for (unsigned int i = 0; i < numChildren; i++)
		{
			FnKat::Attribute childAttr = projectionArbitraryAttribute.getChildByIndex(i);
			
			// only process floats for the moment...
			if (childAttr.getType() == kFnKatAttributeTypeFloat)
			{
				FnKat::FloatAttribute typedAttr = childAttr;
				if (typedAttr.isValid())
				{
					std::string childName = projectionArbitraryAttribute.getChildName(i);
					pRenderCamera->addFloatParam(childName, typedAttr.getValue(0.0f, false));
				}
			}
		}
	}

	m_pScene->setDefaultCamera(pRenderCamera);
}

void ImagineRender::buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	// force expand, using procedurals isn't worth it in this day and age, especially as there's a fair amount of memory overhead for the state...

	SGLocationProcessor locProcessor(*m_pScene, m_creationSettings);
		
	if (m_enableIDPicking)
	{
		locProcessor.initIDState(m_rawKatanaHost, m_aInteractiveFrameIDs[0]);
	}

	locProcessor.processSGForceExpand(rootIterator);

	// add materials lazily
	std::vector<Material*> aMaterials;
	locProcessor.getFinalMaterials(aMaterials);

	MaterialManager& mm = m_pScene->getMaterialManager();

	mm.addMaterialsLazy(aMaterials);
}

void ImagineRender::enforceSaneSceneSetup()
{
	// if there's no light in the scene and we're not doing direct illumination, add a physical sky so we at least see something (and is useful for debug renders)...
	if (m_pScene->getLightCount() == 0 && !(m_integratorType == 0 && m_ambientOcclusion))
	{
		PhysicalSky* pPhysicalSkyLight = new PhysicalSky();

		pPhysicalSkyLight->setRadius(1000.0f);

		m_pScene->addObject(pPhysicalSkyLight, false, false, false);
	}
}

void ImagineRender::performDiskRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	if (m_diskRenderOutputPath.empty())
		return;

	fprintf(stderr, "Performing disk render to: %s\n", m_diskRenderOutputPath.c_str());

	{
		Timer t1("Katana scene expansion");
		buildSceneGeometry(settings, rootIterator);
	}

	enforceSaneSceneSetup();

	flushCaches();

	// assumes render settings have been set correctly before hand...

	fprintf(stderr, "Imagine: Starting Disk render...\n");

	startDiskRenderer();

	renderFinished();
}

void ImagineRender::performPreviewRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	if (!setupPreviewDataChannel(settings))
		return;

	{
		Timer t1("Katana scene expansion");
		buildSceneGeometry(settings, rootIterator);
	}

	enforceSaneSceneSetup();

	flushCaches();

	// assumes render settings have been set correctly before hand...

	fprintf(stderr, "Imagine: Starting Preview render...\n");

	startInteractiveRenderer(false);

	renderFinished();
}

void ImagineRender::performLiveRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	if (!setupPreviewDataChannel(settings))
		return;

	{
		Timer t1("Katana scene expansion");
		buildSceneGeometry(settings, rootIterator);
	}

	enforceSaneSceneSetup();

	flushCaches();

	// assumes render settings have been set correctly before hand...

	// but we add/override preview settings...
	m_renderSettings.add("preview", true);
	if (m_fastLiveRenders)
	{
		m_renderSettings.add("SamplesPerIteration", 4);
		m_renderSettings.add("Iterations", 60);
	}
	else
	{
		m_renderSettings.add("SamplesPerIteration", 16);
		m_renderSettings.add("Iterations", 30);
	}
	m_renderSettings.add("progressive", true);

	m_renderSettings.add("integrated_rerender", true);

	fprintf(stderr, "Imagine: Starting Live render...\n");

	startInteractiveRenderer(true);

//	renderFinished();
}

void ImagineRender::flushCaches()
{
	fprintf(stderr, "Imagine: Scene expansion complete - flushing caches...\n");

	FnKat::RenderOutputUtils::flushProceduralDsoCaches();

	// call mallocTrim in order to free up any memory that hasn't been de-allocated yet.
	// Katana can be pretty inefficient with all its std::vectors it allocates for attributes,
	// fragmenting the heap quite a bit...

	mallocTrim();
}

void ImagineRender::startDiskRenderer()
{
	unsigned int imageFlags = COMPONENT_RGBA | COMPONENT_SAMPLES;
	imageFlags |= m_extraAOVsFlags;
	unsigned int imageChannelWriteFlags = ImageWriter::ALL;

	unsigned int writeFlags = 0;

	// check that we'll be able to write the image before we actually render...
	// TODO: this should be done before we bother expanding!!!

	std::string directory = FileHelpers::getFileDirectory(m_diskRenderOutputPath);

	if (!FileHelpers::doesDirectoryExist(directory))
	{
		fprintf(stderr, "Error: The specified output directory for the file does not exist.\n");
		return;
	}

	std::string extension = FileHelpers::getFileExtension(m_diskRenderOutputPath);

	ImageWriter* pWriter = FileIORegistry::instance().createImageWriterForExtension(extension);
	if (!pWriter)
	{
		fprintf(stderr, "Error: The output file extension '%s' was not recognised.\n", extension.c_str());
		return;
	}

	OutputImage renderImage(m_renderWidth, m_renderHeight, imageFlags);

	// for the moment, use the number of render threads for the number of worker threads to use.
	// this effects things like parallel accel structure building, tesselation and reading of textures when in
	// non-lazy mode...
	GlobalContext::instance().setWorkerThreads(m_renderThreads);

	// we don't want progressive rendering...
	Raytracer raytracer(*m_pScene, &renderImage, m_renderSettings, false, m_renderThreads);
	raytracer.setHost(this);

	raytracer.setStatisticsOutputPath(m_statsOutputPath);

	renderImage.clearImage();

	raytracer.renderScene(1.0f, NULL, true);

	m_rendererOtherMemory = raytracer.getRendererMemoryUsage();

	renderImage.normaliseProgressive();

	if (m_diskRenderConvertFromLinear)
	{
		// TODO: this doesn't really make sense as we're writing to .exr which should be in linear anyway, but doing this
		// matches other renderers... Need to fix this properly at some point, but should only affect light AOVs anyway...
		renderImage.applyExposure(2.2f);
	}
	else
	{
		renderImage.applyExposure(1.1f);
	}

	pWriter->writeImage(m_diskRenderOutputPath, renderImage, imageChannelWriteFlags, writeFlags);

	delete pWriter;
}

void ImagineRender::startInteractiveRenderer(bool liveRender)
{
#if ENABLE_PREVIEW_RENDERS
	
	unsigned int imageFlags = COMPONENT_RGBA | COMPONENT_SAMPLES;
	if (m_enableIDPicking)
	{
		imageFlags |= COMPONENT_ID;
	}
	imageFlags |= m_extraAOVsFlags;

	if (!m_ROIActive)
	{
		m_pOutputImage = new OutputImage(m_renderWidth, m_renderHeight, imageFlags);
	}
	else
	{
		m_pOutputImage = new OutputImage(m_ROIWidth, m_ROIHeight, imageFlags);
	}
	m_pOutputImage->clearImage();

	// for the moment, use the number of render threads for the number of worker threads to use.
	// this effects things like parallel accel structure building, tesselation and reading of textures when in
	// non-lazy mode...
	GlobalContext::instance().setWorkerThreads(m_renderThreads);

	if (!liveRender)
	{
		// we don't want progressive rendering...
		Raytracer raytracer(*m_pScene, m_pOutputImage, m_renderSettings, false, m_renderThreads);
		raytracer.setHost(this);
		raytracer.setStatisticsOutputPath(m_statsOutputPath);

		if (m_integratorType == 0 && m_ambientOcclusion)
		{
			raytracer.setAmbientColour(Colour3f(0.7f));
		}

		raytracer.renderScene(1.0f, NULL, true);

		m_rendererOtherMemory = raytracer.getRendererMemoryUsage();
	}
	else
	{
		// it is a live render, so we need to keep a Raytracer instance around in order to be able to cancel it when
		// something changes

		if (m_pRaytracer)
		{
			delete m_pRaytracer;
			m_pRaytracer = NULL;
		}

		m_pRaytracer = new Raytracer(*m_pScene, m_renderThreads, true);
		m_pRaytracer->setExtraChannels(0);
		m_pRaytracer->setHost(this);
		m_pRaytracer->setStatisticsOutputPath(m_statsOutputPath);

		if (m_integratorType == 0 && m_ambientOcclusion)
		{
			m_pRaytracer->setAmbientColour(Colour3f(0.7f));
		}

		m_pRaytracer->initialise(m_pOutputImage, m_renderSettings);

		m_pRaytracer->renderScene(1.0f, &m_renderSettings, false);

//		m_rendererOtherMemory = m_pRaytracer->getRendererMemoryUsage();
	}
#endif
}

void ImagineRender::renderFinished()
{
	// TODO: see if this is getting called on re-render...
#if ENABLE_PREVIEW_RENDERS
	sendFullFrameToMonitor();
#endif
	
	fprintf(stderr, "Render complete.\n");

	if (m_printMemoryStatistics != 0)
	{
		// run through all objects in the scene, building up geometry info

		GeometryInfo info;
		m_pScene->getContentsGeometryInfo(info);

		fprintf(stderr, "\n\nMemory Statistics:\n-----------\n");

		std::string totalSourceGeoSize = formatSize(info.getTotalSourceSize());
		std::string uniqueSourceGeoSize = formatSize(info.getUniqueSourceSize());
		std::string totalTrianglesSize = formatSize(info.getTotalTrianglesSize());
		std::string uniqueTrianglesSize = formatSize(info.getUniqueTrianglesSize());
		std::string totalTriangleIndicesSize = formatSize(info.getTotalTriangleIndicesSize());
		std::string uniqueTriangleIndicesSize = formatSize(info.getUniqueTriangleIndicesSize());
		std::string otherSize = formatSize(info.getOtherSize() + m_rendererOtherMemory);
		std::string accelSize = formatSize(info.getAccelerationStructureSize());

		unsigned int numImages = 0;
		size_t imageTextureSize = ImageCache::instance().getImageCacheMemorySize(&numImages);
		std::string strImageTextureSize = formatSize(imageTextureSize);

		fprintf(stderr, "Total source Geo memory size: %s\n", totalSourceGeoSize.c_str());
		fprintf(stderr, "Unique source Geo memory size: %s\n", uniqueSourceGeoSize.c_str());

		std::string totalTrianglesCount = formatNumberThousandsSeparator(info.getTotalTrianglesCount());
		std::string uniqueTrianglesCount = formatNumberThousandsSeparator(info.getUniqueTrianglesCount());
		fprintf(stderr, "Total triangle count: %s, total triangles memory size: %s\n", totalTrianglesCount.c_str(), totalTrianglesSize.c_str());
		fprintf(stderr, "Unique triangle count: %s, unique triangles memory size: %s\n", uniqueTrianglesCount.c_str(), uniqueTrianglesSize.c_str());
		fprintf(stderr, "Total triangle indices memory size: %s\n", totalTriangleIndicesSize.c_str());
		fprintf(stderr, "Unique triangle indices memory size: %s\n", uniqueTriangleIndicesSize.c_str());
		fprintf(stderr, "Total other memory size: %s\n", otherSize.c_str());

		if (m_printMemoryStatistics == 2)
		{
			fprintf(stderr, "\nHistograms:");

			fprintf(stderr, "Total num meshes: %u\n", info.getMeshCount());

			unsigned int meshTrianglesUChar, meshTrianglesUShort, meshTrianglesUInt;
			info.getMeshTrianglesCounts(meshTrianglesUChar, meshTrianglesUShort, meshTrianglesUInt);
			fprintf(stderr, "Triangle counts:\nuchar:\t\t%u\nushort:\t\t%u\nuint:\t\t%u\n\n", meshTrianglesUChar, meshTrianglesUShort, meshTrianglesUInt);

			unsigned int meshPointsUChar, meshPointsUShort, meshPointsUInt;
			info.getMeshPointsCounts(meshPointsUChar, meshPointsUShort, meshPointsUInt);
			fprintf(stderr, "Point counts:\nuchar:\t\t%u\nushort:\t\t%u\nuint:\t\t%u\n\n", meshPointsUChar, meshPointsUShort, meshPointsUInt);

			unsigned int meshNormalsUChar, meshNormalsUShort, meshNormalsUInt;
			info.getMeshNormalsCounts(meshNormalsUChar, meshNormalsUShort, meshNormalsUInt);
			fprintf(stderr, "Normal counts:\nuchar:\t\t%u\nushort:\t\t%u\nuint:\t\t%u\n\n", meshNormalsUChar, meshNormalsUShort, meshNormalsUInt);

			unsigned int meshUVsUChar, meshUVsUShort, meshUVsUInt;
			info.getMeshUVsCounts(meshUVsUChar, meshUVsUShort, meshUVsUInt);
			fprintf(stderr, "UV counts:\nuchar:\t\t%u\nushort:\t\t%u\nuint:\t\t%u\n\n", meshUVsUChar, meshUVsUShort, meshUVsUInt);

			fprintf(stderr, "Full Normals count:\t%u\n", info.getFullNormalsCount());
			fprintf(stderr, "Full UVs count:\t%u\n\n", info.getFullUVsCount());
		}

		fprintf(stderr, "Total accel structure memory size: %s\n", accelSize.c_str());
		fprintf(stderr, "Total image texture count: %u, total image texture memory size: %s\n\n", numImages, strImageTextureSize.c_str());
	}
}

void ImagineRender::restartLiveRender()
{
	m_liveRenderState.lock();

	::usleep(500);

	m_pOutputImage->clearImage();
/*
	if (m_pRaytracer)
	{
		delete m_pRaytracer;
		m_pRaytracer = NULL;
	}

	m_pRaytracer = new Raytracer(*m_pScene, m_renderThreads, true);
	m_pRaytracer->setExtraChannels(0);
	m_pRaytracer->setHost(this);

	m_pRaytracer->initialise(m_pOutputImage, m_renderSettings);
*/
	m_liveRenderState.unlock();

	m_pRaytracer->renderScene(1.0f, &m_renderSettings, false);
}

// progress back from the main renderer class
void ImagineRender::progressChanged(float progress)
{
	int iProgress = (int)progress;

	if (iProgress >= m_lastProgress + 5)
	{
		m_lastProgress = iProgress;
		fprintf(stderr, "Render progress: %d%%\n", iProgress);
	}
}

DEFINE_RENDER_PLUGIN(ImagineRender)

void registerPlugins()
{
	REGISTER_PLUGIN(ImagineRender, "imagine", 0, 1);
}
