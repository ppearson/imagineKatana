#include "imagine_render.h"

#include <stdio.h>

#include <Render/GlobalSettings.h>
#include <RendererInfo/RenderMethod.h>
#include <RenderOutputUtils/RenderOutputUtils.h>

#ifndef STAND_ALONE
// include any Imagine headers directly from the source directory as Imagine hasn't got an API yet...
#include "objects/camera.h"
#include "image/output_image.h"
#include "io/image/image_writer_exr.h"

#include "lights/physical_sky.h"

#include "raytracer/raytracer.h"

#include "utils/file_helpers.h"
#include "utils/memory.h"
#include "utils/string_helpers.h"

#include "global_context.h"

#endif

#include "utilities.h"

#include "sg_location_processor.h"

ImagineRender::ImagineRender(FnKat::FnScenegraphIterator rootIterator, FnKat::GroupAttribute arguments) :
	RenderBase(rootIterator, arguments), m_pScene(NULL), m_useCompactGeometry(true), m_deduplicateVertexNormals(false), m_printStatistics(false)
{
#if ENABLE_PREVIEW_RENDERS
	m_pOutputImage = NULL;
	m_pDataPipe = NULL;
	m_pFrame = NULL;
	m_pChannel = NULL;
#endif

	m_renderWidth = 512;
	m_renderHeight = 512;
}

int ImagineRender::start()
{
//	system("xmessage test1\n");

	FnKat::FnScenegraphIterator rootIterator = getRootIterator();

	std::string renderMethodName = getRenderMethodName();

	// Batch renders are just disk renders really...
	if (renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kBatchName)
	{
		renderMethodName = FnKat::RendererInfo::DiskRenderMethod::kDefaultName;
	}

	m_lastProgress = 0;

	Utilities::registerFileReaders();

	float renderFrame = getRenderTime();

	// create the scene - this is going to leak for the moment...
	m_pScene = new Scene(true); // don't want a GUI with OpenGL...

	// we want to do things like bbox calculation, vertex normal calculation (for Subdiv approximations) and triangle tessellation
	// in parallel across multiple threads
	m_pScene->setParallelGeoBuild(true);

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
	m_renderWidth = settings.getResolutionX();
	m_renderHeight = settings.getResolutionY();

//	fprintf(stderr, "Render dimensions: %d, %d\n", m_renderWidth, m_renderHeight);

#ifndef STAND_ALONE
	m_renderSettings.add("width", m_renderWidth);
	m_renderSettings.add("height", m_renderHeight);
#endif

	// work out number of threads to use for rendering
	settings.applyRenderThreads(m_renderThreads);
	applyRenderThreadsOverride(m_renderThreads);

	Foundry::Katana::StringAttribute cameraNameAttribute = rootIterator.getAttribute("renderSettings.cameraName");
	std::string cameraName = cameraNameAttribute.getValue("/root/world/cam/camera", false);

	FnKat::FnScenegraphIterator cameraIterator = rootIterator.getByPath(cameraName);
	if (!cameraIterator.isValid())
	{
		fprintf(stderr, "Error: Can't get hold of render camera attributes...\n");
		return false;
	}

	buildCamera(settings, cameraIterator);

	configureRenderSettings(settings, rootIterator);

	return true;
}

bool ImagineRender::configureRenderSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	FnKat::GroupAttribute renderSettingsAttribute = rootIterator.getAttribute("imagineGlobalStatements");

	// TODO: provide helper wrapper function to do all this isValue() and default value stuff.
	//       It's rediculous that Katana doesn't provide this anyway...

	FnKat::IntAttribute integratorTypeAttribute = renderSettingsAttribute.getChildByName("integrator");
	unsigned int integratorType = 1;
	if (integratorTypeAttribute.isValid())
		integratorType = integratorTypeAttribute.getValue(1, false);

	FnKat::IntAttribute volumetricsAttribute = renderSettingsAttribute.getChildByName("enable_volumetrics");
	unsigned int volumetrics = 0;
	if (volumetricsAttribute.isValid())
		volumetrics = volumetricsAttribute.getValue(0, false);

	FnKat::IntAttribute samplesPerPixelAttribute = renderSettingsAttribute.getChildByName("spp");
	unsigned int samplesPerPixel = 64;
	if (samplesPerPixelAttribute.isValid())
		samplesPerPixel = samplesPerPixelAttribute.getValue(64, false);

	FnKat::IntAttribute filterTypeAttribute = renderSettingsAttribute.getChildByName("reconstruction_filter");
	unsigned int filterType = 3;
	if (filterTypeAttribute.isValid())
		filterType = filterTypeAttribute.getValue(3, false);

	// ray depths

	FnKat::IntAttribute maxDepthOverallAttribute = renderSettingsAttribute.getChildByName("max_depth_overall");
	unsigned int maxDepthOverall = 4;
	if (maxDepthOverallAttribute.isValid())
		maxDepthOverall = maxDepthOverallAttribute.getValue(4, false);

	FnKat::IntAttribute maxDepthDiffuseAttribute = renderSettingsAttribute.getChildByName("max_depth_diffuse");
	unsigned int maxDepthDiffuse = 3;
	if (maxDepthDiffuseAttribute.isValid())
		maxDepthDiffuse = maxDepthDiffuseAttribute.getValue(3, false);

	FnKat::IntAttribute maxDepthGlossyAttribute = renderSettingsAttribute.getChildByName("max_depth_glossy");
	unsigned int maxDepthGlossy = 3;
	if (maxDepthGlossyAttribute.isValid())
		maxDepthGlossy = maxDepthGlossyAttribute.getValue(3, false);

	FnKat::IntAttribute maxDepthRefractionAttribute = renderSettingsAttribute.getChildByName("max_depth_refraction");
	unsigned int maxDepthRefraction = 5;
	if (maxDepthRefractionAttribute.isValid())
		maxDepthRefraction = maxDepthRefractionAttribute.getValue(5, false);

	FnKat::IntAttribute maxDepthReflectionAttribute = renderSettingsAttribute.getChildByName("max_depth_reflection");
	unsigned int maxDepthReflection = 5;
	if (maxDepthRefractionAttribute.isValid())
		maxDepthReflection = maxDepthReflectionAttribute.getValue(5, false);


	// PathDist stuff
	if (integratorType == 2)
	{
		unsigned int diffuseMultipler = 3;
		unsigned int glossyMultipler = 2;

		FnKat::IntAttribute diffuseMultiplerAttribute = renderSettingsAttribute.getChildByName("path_dist_diffuse_multiplier");
		if (diffuseMultiplerAttribute.isValid())
			diffuseMultipler = diffuseMultiplerAttribute.getValue(3, false);

		FnKat::IntAttribute glossyMultiplerAttribute = renderSettingsAttribute.getChildByName("path_dist_glossy_multiplier");
		if (glossyMultiplerAttribute.isValid())
			glossyMultipler = glossyMultiplerAttribute.getValue(2, false);

		m_renderSettings.add("pathDistDiffuseMult", diffuseMultipler);
		m_renderSettings.add("pathDistGlossyMult", glossyMultipler);
	}


	//
	FnKat::IntAttribute bakeDownSceneAttribute = renderSettingsAttribute.getChildByName("bake_down_scene");
	unsigned int bakeDownScene = 1;
	if (bakeDownSceneAttribute.isValid())
		bakeDownScene = bakeDownSceneAttribute.getValue(1, false);

	FnKat::IntAttribute useCompactGeometryAttribute = renderSettingsAttribute.getChildByName("use_compact_geometry");
	unsigned int useCompactGeometry = 1;
	if (useCompactGeometryAttribute.isValid())
		useCompactGeometry = useCompactGeometryAttribute.getValue(1, false);
	m_useCompactGeometry = (useCompactGeometry == 1);

	FnKat::IntAttribute deduplicateVertexNormalsAttribute = renderSettingsAttribute.getChildByName("deduplicate_vertex_normals");
	m_deduplicateVertexNormals = false;
	if (deduplicateVertexNormalsAttribute.isValid())
		m_deduplicateVertexNormals = (deduplicateVertexNormalsAttribute.getValue(0, false) == 1);

	FnKat::IntAttribute printStatisticsAttribute = renderSettingsAttribute.getChildByName("print_statistics");
	m_printStatistics = true;
	if (printStatisticsAttribute.isValid())
		m_printStatistics = (printStatisticsAttribute.getValue(1, false) == 1);

	FnKat::FloatAttribute rayEpsilonAttribute = renderSettingsAttribute.getChildByName("ray_epsilon");
	float rayEpsilon = 0.001f;
	if (rayEpsilonAttribute.isValid())
		rayEpsilon = rayEpsilonAttribute.getValue(0.001f, false);

	FnKat::IntAttribute bucketOrderAttribute = renderSettingsAttribute.getChildByName("bucket_order");
	unsigned int bucketOrder = 2;
	if (bucketOrderAttribute.isValid())
		bucketOrder = bucketOrderAttribute.getValue(2, false);

	FnKat::IntAttribute bucketSizeAttribute = renderSettingsAttribute.getChildByName("bucket_size");
	unsigned int bucketSize = 32;
	if (bucketSizeAttribute.isValid())
		bucketSize = bucketSizeAttribute.getValue(32, false);

#ifndef STAND_ALONE
	m_renderSettings.add("integrator", integratorType);
	m_renderSettings.add("rayEpsilon", rayEpsilon);

// for the moment, only do one iteration, at least for disk renders...
	m_renderSettings.add("Iterations", 1);
	m_renderSettings.add("volumetric", volumetrics == 1); // this needs to be a bool...
	m_renderSettings.add("SamplesPerIteration", samplesPerPixel);

	m_renderSettings.add("filter_type", filterType);

	m_renderSettings.add("rbOverall", maxDepthOverall);
	m_renderSettings.add("rbDiffuse", maxDepthDiffuse);
	m_renderSettings.add("rbGlossy", maxDepthGlossy);
	m_renderSettings.add("rbRefraction", maxDepthRefraction);
	m_renderSettings.add("rbReflection", maxDepthReflection);

	m_renderSettings.add("tile_order", bucketOrder);
	m_renderSettings.add("tile_size", bucketSize);

	if (bakeDownScene == 1)
	{
		m_pScene->setBakeDownScene(true);
	}
#endif

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

void ImagineRender::buildCamera(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator cameraIterator)
{
	FnKat::GroupAttribute cameraGeometryAttribute = cameraIterator.getAttribute("geometry");
	if (!cameraGeometryAttribute.isValid())
	{
		fprintf(stderr, "Error: Can't find geometry attribute on the camera...\n");
		return;
	}

	FnKat::StringAttribute cameraProjectionTypeAttribute = cameraGeometryAttribute.getChildByName("projection");

	// screen window - for the moment, it's just the render res, with no overscan or crop....
	FnKat::DoubleAttribute leftAttrib = cameraGeometryAttribute.getChildByName("left");
	FnKat::DoubleAttribute rightAttrib = cameraGeometryAttribute.getChildByName("right");
	FnKat::DoubleAttribute topAttrib = cameraGeometryAttribute.getChildByName("top");
	FnKat::DoubleAttribute bottomAttrib = cameraGeometryAttribute.getChildByName("bottom");

	FnKat::DoubleAttribute fovAttribute = cameraGeometryAttribute.getChildByName("fov");

	// TODO: get time samples for MB...
	float fovValue = fovAttribute.getValue(45.0f, false);

//	fprintf(stderr, "Camera FOV: %f\n", fovValue);

	// TODO: other camera attribs

	// get Camera transform matrix

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(cameraIterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

//	fprintf(stderr, "Camera Matrix:\n%f, %f, %f, %f,\n%f, %f, %f, %f,\n%f, %f, %f, %f,\n%f, %f, %f, %f\n", pMatrix[0], pMatrix[1], pMatrix[2], pMatrix[3],
//			pMatrix[4], pMatrix[5], pMatrix[6], pMatrix[7], pMatrix[8], pMatrix[9], pMatrix[10], pMatrix[11], pMatrix[12], pMatrix[13],
//			pMatrix[14], pMatrix[15]);

#ifndef STAND_ALONE
	Camera* pRenderCamera = new Camera();
	pRenderCamera->setFOV(fovValue);
	pRenderCamera->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_pScene->setDefaultCamera(pRenderCamera);

#endif
}

void ImagineRender::buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	// force expand for the moment, instead of using lazy procedurals...

#ifndef STAND_ALONE
	SGLocationProcessor locProcessor(*m_pScene);
#else
	SGLocationProcessor locProcessor;
#endif

	locProcessor.setUseCompactGeometry(m_useCompactGeometry);
	locProcessor.setDeduplicateVertexNormals(m_deduplicateVertexNormals);

	locProcessor.processSGForceExpand(rootIterator);
}

void ImagineRender::performDiskRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	if (m_diskRenderOutputPath.empty())
		return;

	fprintf(stderr, "Performing disk render to: %s\n", m_diskRenderOutputPath.c_str());

	buildSceneGeometry(settings, rootIterator);

	// if there's no light in the scene, add a physical sky...
	if (m_pScene->getLightCount() == 0)
	{
		PhysicalSky* pPhysicalSkyLight = new PhysicalSky();

		pPhysicalSkyLight->setRadius(1000.0f);

		m_pScene->addObject(pPhysicalSkyLight, false, false);
	}

	// call mallocTrim in order to free up any memory that hasn't been de-allocated yet.
	// Katana can be pretty inefficient with all its std::vectors it allocates for attributes,
	// fragmenting the heap quite a bit...

	mallocTrim();

	fprintf(stderr, "Scene setup complete - starting render.\n");

	// assumes render settings have been set correctly before hand...

	startDiskRenderer();

	renderFinished();
}

void ImagineRender::performPreviewRender(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	std::string katHost = getKatanaHost();

	size_t portSep = katHost.find(":");

	if (portSep == std::string::npos)
	{
		fprintf(stderr, "Error: Katana host and port not specified...\n");
		return;
	}

	m_katanaHost = katHost.substr(0, portSep);

	std::string strPort = katHost.substr(portSep + 1);
	m_katanaPort = atol(strPort.c_str());

	m_katanaPort += 100;

	// perversely, Katana requires the port number retrieved above to be incremented by 100,
	// despite the fact that KatanaPipe::connect() seems to handle doing that itself for the control
	// port (which is +100 the data port). Not doing this causes Katana to segfault on the connect()
	// call, leaving renderBoot orphaned doing the render in the background...

	//
#if ENABLE_PREVIEW_RENDERS
	FnKat::Render::RenderSettings::ChannelBuffers interactiveRenderBuffers;
	settings.getChannelBuffers(interactiveRenderBuffers);

	FnKat::Render::RenderSettings::ChannelBuffers::const_iterator itBuffer = interactiveRenderBuffers.begin();
	for (; itBuffer != interactiveRenderBuffers.end(); ++itBuffer)
	{
		const FnKat::Render::RenderSettings::ChannelBuffer& buffer = (*itBuffer).second;

		m_frameID = atoi(buffer.bufferId.c_str());

		m_channelName = buffer.channelName;

		fprintf(stderr, "Buffers: %i, %s\n", m_frameID, m_channelName.c_str());
	}


	//

	m_pDataPipe = FnKat::PipeSingleton::Instance(m_katanaHost, m_katanaPort);

	// try and connect...
	if (m_pDataPipe->connect() != 0)
	{
		fprintf(stderr, "Can't connect to Katana data socket: %s:%u\n", m_katanaHost.c_str(), m_katanaPort);
		return;
	}

	m_pFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, 0, 0);

	// set the name
	std::string frameName;
	FnKat::encodeLegacyName(m_channelName, m_frameID, frameName);
	m_pFrame->setFrameName(frameName);

	m_pDataPipe->send(*m_pFrame);

	int channelID = 0;
	m_pChannel = new FnKat::NewChannelMessage(*m_pFrame, channelID, m_renderHeight, m_renderWidth, 0, 0, 1.0f, 1.0f);

	std::string channelName;
	FnKat::encodeLegacyName(m_channelName, channelID, channelName);
	m_pChannel->setChannelName(channelName);

	// total size of a pixel for a single channel (RGBA * float) = 16
	// this is used by Katana to work out how many Channels there are in the image...
	m_pChannel->setDataSize(sizeof(float) * 4);

	m_pDataPipe->send(*m_pChannel);
#endif

	buildSceneGeometry(settings, rootIterator);

	// if there's no light in the scene, add a physical sky...
	if (m_pScene->getLightCount() == 0)
	{
		PhysicalSky* pPhysicalSkyLight = new PhysicalSky();

		pPhysicalSkyLight->setRadius(1000.0f);

		m_pScene->addObject(pPhysicalSkyLight, false, false);
	}

	// call mallocTrim in order to free up any memory that hasn't been de-allocated yet.
	// Katana can be pretty inefficient with all its std::vectors it allocates for attributes,
	// fragmenting the heap quite a bit...

	mallocTrim();

	fprintf(stderr, "Scene setup complete - starting render.\n");

	// assumes render settings have been set correctly before hand...

	startInteractiveRenderer();

	renderFinished();
}

void ImagineRender::startDiskRenderer()
{
	unsigned int imageFlags = COMPONENT_RGBA | COMPONENT_SAMPLES;
	unsigned int imageChannelWriteFlags = ImageWriter::ALL;

	unsigned int writeFlags = 0;

	// check that we'll be able to write the image before we actually render...

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

	renderImage.clearImage();

	raytracer.renderScene(1.0f, NULL);

	renderImage.normaliseProgressive();
	renderImage.applyExposure(1.8f);

	pWriter->writeImage(m_diskRenderOutputPath, renderImage, imageChannelWriteFlags, writeFlags);

	delete pWriter;
}

void ImagineRender::startInteractiveRenderer()
{
#if ENABLE_PREVIEW_RENDERS
	unsigned int imageFlags = COMPONENT_RGBA | COMPONENT_SAMPLES;

	m_pOutputImage = new OutputImage(m_renderWidth, m_renderHeight, imageFlags);
	m_pOutputImage->clearImage();

	// for the moment, use the number of render threads for the number of worker threads to use.
	// this effects things like parallel accel structure building, tesselation and reading of textures when in
	// non-lazy mode...
	GlobalContext::instance().setWorkerThreads(m_renderThreads);

	// we don't want progressive rendering...
	Raytracer raytracer(*m_pScene, m_pOutputImage, m_renderSettings, false, m_renderThreads);
	raytracer.setHost(this);

	raytracer.renderScene(1.0f, NULL);
#endif
}

void ImagineRender::renderFinished()
{
	fprintf(stderr, "Render complete.\n");

	if (m_printStatistics)
	{
		// run through all objects in the scene, building up geometry info - this isn't really correct in a normal
		// Imagine situation, as we should use renderableObjects to get only visible and final geometry (subdived and displaced).
		// But in this case with integrating into Katana, it makes sense for the moment, and allows useful comparisons
		// with other renderers.

		unsigned int numObjects = m_pScene->getObjectCount();

		GeometryInfo info;
		for (unsigned int i = 0; i < numObjects; i++)
		{
			const Object* pObject = m_pScene->getObject(i);

			const GeometryInstance* pGI = pObject->getGeometryInstance();

			if (!pGI)
				continue;

			info.addInstance(pGI);
		}

		fprintf(stderr, "\n\nGeometry Statistics:\n");

		std::string trianglesSize = formatSize(info.getTotalTrianglesSize());

		fprintf(stderr, "Total triangle count: %u, total triangles memory size: %s\n\n", info.getTotalTrianglesCount(), trianglesSize.c_str());
	}

#if ENABLE_PREVIEW_RENDERS
	if (m_pOutputImage)
	{
		delete m_pOutputImage;
		m_pOutputImage = NULL;
	}

#endif
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

void ImagineRender::tileDone(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int threadID)
{
#if ENABLE_PREVIEW_RENDERS
	if (m_pOutputImage)
	{
		OutputImage imageCopy(*m_pOutputImage);

		imageCopy.normaliseProgressive();
		imageCopy.applyExposure(1.8f);

		FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(m_pChannel));
		pNewTileMessage->setStartCoordinates(x, y);
		pNewTileMessage->setDataDimensions(width, height);

		unsigned int skipSize = 4 * sizeof(float);
		unsigned int dataSize = width * height * skipSize;
		unsigned char* pData = new unsigned char[dataSize];

		unsigned char* pDstRow = pData;

		for (unsigned int i = 0; i < height; i++)
		{
			const Colour4f* pSrcRow = imageCopy.colourRowPtr(y + i);
			const Colour4f* pSrcPixel = pSrcRow + x;

			unsigned char* pDstPixel = pDstRow;

			for (unsigned int j = 0; j < width; j++)
			{
				// copy each component manually, as we need to swap the A channel to be beginning for Katana...
				memcpy(pDstPixel, &pSrcPixel->a, sizeof(float));
				pDstPixel += sizeof(float);
				memcpy(pDstPixel, &pSrcPixel->r, sizeof(float));
				pDstPixel += sizeof(float);
				memcpy(pDstPixel, &pSrcPixel->g, sizeof(float));
				pDstPixel += sizeof(float);
				memcpy(pDstPixel, &pSrcPixel->b, sizeof(float));
				pDstPixel += sizeof(float);

				pSrcPixel += 1;
			}

			pDstRow += width * skipSize;
		}

		pNewTileMessage->setData(pData, dataSize);
		pNewTileMessage->setByteSkip(skipSize);

		m_pDataPipe->send(*pNewTileMessage);

		// we can now delete the data (but not the message)
		delete [] pData;
		pData = NULL;
	}
#endif
}

DEFINE_RENDER_PLUGIN(ImagineRender)

void registerPlugins()
{
	REGISTER_PLUGIN(ImagineRender, "imagine", 0, 1);
}
