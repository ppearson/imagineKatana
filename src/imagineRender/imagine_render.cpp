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
	}

	std::string renderMethodName = getRenderMethodName();

	// Batch renders are just disk renders really...
	if (renderMethodName == FnKat::RendererInfo::DiskRenderMethod::kBatchName)
	{
		renderMethodName = FnKat::RendererInfo::DiskRenderMethod::kDefaultName;
	}

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
		fprintf(stderr, "ROI found: (%i, %i, %i, %i)\n", roiValues[0], roiValues[1], roiValues[2], roiValues[3]);

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

// Imagine-specific settings
bool ImagineRender::configureRenderSettings(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator, bool diskRender)
{
	FnKat::GroupAttribute imagineGSAttribute = rootIterator.getAttribute("imagineGlobalStatements");

	KatanaAttributeHelper gsHelper(imagineGSAttribute);

	m_integratorType = gsHelper.getIntParam("integrator", 1);

	unsigned int lightSamplingType = gsHelper.getIntParam("light_sampling", 0);
	unsigned int lightSamples = gsHelper.getIntParam("light_samples", 1);

	unsigned int volumetrics = gsHelper.getIntParam("enable_volumetrics", 0);

	unsigned int mis = gsHelper.getIntParam("use_mis", 1);

	unsigned int evaluateGlossy = gsHelper.getIntParam("evaluate_glossy", 1);
	unsigned int allowIndirectCaustics = gsHelper.getIntParam("allow_indirect_caustics", 0);
	unsigned int russianRoulette = gsHelper.getIntParam("russian_roulette", 1);

	unsigned int useAdaptive = gsHelper.getIntParam("adaptive", 0);

	float adaptiveVarianceThreshold = gsHelper.getFloatParam("adaptive_variance_threshold", 0.01f);

	unsigned int samplesPerPixel = gsHelper.getIntParam("spp", 64);

	FnKat::IntAttribute iterationsAttribute = imagineGSAttribute.getChildByName("iterations");
	unsigned int iterations = 1;
	if (iterationsAttribute.isValid() && !diskRender)
	{
		iterations = iterationsAttribute.getValue(1, false);
		samplesPerPixel /= iterations;
	}

	FnKat::IntAttribute fastLiveRendersAttribute = imagineGSAttribute.getChildByName("fast_live_renders");
	if (fastLiveRendersAttribute.isValid())
	{
		m_fastLiveRenders = (fastLiveRendersAttribute.getValue(0, false) == 1);
	}

	unsigned int filterType = gsHelper.getIntParam("reconstruction_filter", 3);

	// TODO: filter width...

	// ray depths

	unsigned int maxDepthOverall = gsHelper.getIntParam("max_depth_overall", 4);
	unsigned int maxDepthDiffuse = gsHelper.getIntParam("max_depth_diffuse", 3);
	unsigned int maxDepthGlossy = gsHelper.getIntParam("max_depth_glossy", 3);
	unsigned int maxDepthRefraction = gsHelper.getIntParam("max_depth_refraction", 5);
	unsigned int maxDepthReflection = gsHelper.getIntParam("max_depth_reflection", 5);

	// PathDist stuff
	if (m_integratorType == 2)
	{
		unsigned int diffuseMultipler = gsHelper.getIntParam("path_dist_diffuse_multiplier", 3);
		unsigned int glossyMultipler = gsHelper.getIntParam("path_dist_glossy_multiplier", 2);

		m_renderSettings.add("pathDistDiffuseMult", diffuseMultipler);
		m_renderSettings.add("pathDistGlossyMult", glossyMultipler);
	}
	else if (m_integratorType == 0)
	{
		// direct illumination

		// set number of samples to
		unsigned int sampleEdge = (unsigned int)(sqrtf((float)samplesPerPixel));
		m_renderSettings.add("antiAliasing", sampleEdge);

		m_ambientOcclusion = false;
		FnKat::IntAttribute ambientOcclusionAttribute = imagineGSAttribute.getChildByName("enable_ambient_occlusion");
		if (ambientOcclusionAttribute.isValid())
			m_ambientOcclusion = (ambientOcclusionAttribute.getValue(0, false) == 1);

		if (m_ambientOcclusion)
		{
			m_renderSettings.add("ambientOcclusion", true);
			unsigned int ambientOcclusionSamples = 5;
			FnKat::IntAttribute ambientOcclusionSamplesAttribute = imagineGSAttribute.getChildByName("ambient_occlusion_samples");
			if (ambientOcclusionSamplesAttribute.isValid())
				ambientOcclusionSamples = ambientOcclusionSamplesAttribute.getValue(5, false);

			m_renderSettings.add("ambientOcclusionSamples", ambientOcclusionSamples);
		}
	}

	FnKat::IntAttribute depthOfFieldAttribute = imagineGSAttribute.getChildByName("depth_of_field");
	unsigned int depthOfField = 0;
	if (depthOfFieldAttribute.isValid())
		depthOfField = depthOfFieldAttribute.getValue(0, false);

	m_renderSettings.add("depthOfField", (depthOfField == 1)); // needs to be bool

	FnKat::IntAttribute motionBlurAttribute = imagineGSAttribute.getChildByName("motion_blur");
	unsigned int motionBlur = 0;
	if (motionBlurAttribute.isValid())
		motionBlur = motionBlurAttribute.getValue(0, false);

	m_creationSettings.m_motionBlur = (motionBlur == 1);
	m_renderSettings.add("motionBlur", m_creationSettings.m_motionBlur); // needs to be bool

	FnKat::IntAttribute flipTAttribute = imagineGSAttribute.getChildByName("flip_t");
	m_creationSettings.m_flipT = false;
	if (flipTAttribute.isValid())
		m_creationSettings.m_flipT = flipTAttribute.getValue(0, false);

	FnKat::IntAttribute enableSubDAttribute = imagineGSAttribute.getChildByName("enable_subdivision");
	m_creationSettings.m_enableSubdivision = false;
	if (enableSubDAttribute.isValid())
		m_creationSettings.m_enableSubdivision = (enableSubDAttribute.getValue(0, false) == 1);


	FnKat::IntAttribute triangleTypeAttribute = imagineGSAttribute.getChildByName("triangle_type");
	m_creationSettings.m_triangleType = 0;
	if (triangleTypeAttribute.isValid())
	{
		m_creationSettings.m_triangleType = triangleTypeAttribute.getValue(0, false);
	}

	FnKat::IntAttribute geoQuantisationTypeAttribute = imagineGSAttribute.getChildByName("geometry_quantisation_type");
	m_creationSettings.m_geoQuantisationType = 0;
	if (geoQuantisationTypeAttribute.isValid())
	{
		m_creationSettings.m_geoQuantisationType = geoQuantisationTypeAttribute.getValue(0, false);
	}

	FnKat::IntAttribute specialisedTriangleTypeAttribute = imagineGSAttribute.getChildByName("specialised_triangle_type");
	m_creationSettings.m_specialisedTriangleType = 0;
	if (specialisedTriangleTypeAttribute.isValid())
	{
		m_creationSettings.m_specialisedTriangleType = specialisedTriangleTypeAttribute.getValue(0, false);
	}

	//
	FnKat::IntAttribute bakeDownSceneAttribute = imagineGSAttribute.getChildByName("bake_down_scene");
	unsigned int bakeDownScene = 0;
	if (bakeDownSceneAttribute.isValid())
		bakeDownScene = bakeDownSceneAttribute.getValue(0, false);

	FnKat::IntAttribute deduplicateVertexNormalsAttribute = imagineGSAttribute.getChildByName("deduplicate_vertex_normals");
	m_creationSettings.m_deduplicateVertexNormals = false;
	if (deduplicateVertexNormalsAttribute.isValid())
		m_creationSettings.m_deduplicateVertexNormals = (deduplicateVertexNormalsAttribute.getValue(0, false) == 1);

	FnKat::IntAttribute useGeoAttrNormalsAttribute = imagineGSAttribute.getChildByName("use_geo_normals");
	m_creationSettings.m_useGeoNormals = true;
	if (useGeoAttrNormalsAttribute.isValid())
		m_creationSettings.m_useGeoNormals = (useGeoAttrNormalsAttribute.getValue(1, false) == 1);

	FnKat::IntAttribute useBoundsAttribute = imagineGSAttribute.getChildByName("use_location_bounds");
	if (useBoundsAttribute.isValid())
		m_creationSettings.m_useBounds = (useBoundsAttribute.getValue(1, false) == 1);

	FnKat::IntAttribute specialiseTypeAttribute = imagineGSAttribute.getChildByName("specialise_types");
	if (specialiseTypeAttribute.isValid())
		m_creationSettings.m_specialiseType = (CreationSettings::SpecialiseType)specialiseTypeAttribute.getValue(0, false);

	FnKat::IntAttribute sceneAccelStructureAttribute = imagineGSAttribute.getChildByName("scene_accel_structure");
	unsigned int sceneAccelStructure = 1;
	if (sceneAccelStructureAttribute.isValid())
		sceneAccelStructure = sceneAccelStructureAttribute.getValue(1, false);

	FnKat::IntAttribute statisticsTypeAttribute = imagineGSAttribute.getChildByName("statistics_type");
	unsigned int statisticsType = 1;
	if (statisticsTypeAttribute.isValid())
		statisticsType = statisticsTypeAttribute.getValue(1, false);

	FnKat::IntAttribute statisticsOutputTypeAttribute = imagineGSAttribute.getChildByName("statistics_output_type");
	unsigned int statisticsOutputType = 0;
	if (statisticsOutputTypeAttribute.isValid())
		statisticsOutputType = statisticsOutputTypeAttribute.getValue(0, false);

	FnKat::StringAttribute statisticsOutputPathAttribute = imagineGSAttribute.getChildByName("statistics_output_path");
	m_statsOutputPath = "";
	if (statisticsOutputPathAttribute.isValid())
		m_statsOutputPath = statisticsOutputPathAttribute.getValue("", false);

	FnKat::IntAttribute printMemoryStatisticsAttribute = imagineGSAttribute.getChildByName("print_memory_statistics");
	m_printMemoryStatistics = 1;
	if (printMemoryStatisticsAttribute.isValid())
		m_printMemoryStatistics = printMemoryStatisticsAttribute.getValue(1, false);

	FnKat::FloatAttribute rayEpsilonAttribute = imagineGSAttribute.getChildByName("ray_epsilon");
	float rayEpsilon = 0.0001f;
	if (rayEpsilonAttribute.isValid())
		rayEpsilon = rayEpsilonAttribute.getValue(0.0001f, false);

	FnKat::FloatAttribute shadowRayEpsilonAttribute = imagineGSAttribute.getChildByName("shadow_ray_epsilon");
	float shadowRayEpsilon = 0.0001f;
	if (shadowRayEpsilonAttribute.isValid())
		shadowRayEpsilon = shadowRayEpsilonAttribute.getValue(0.0001f, false);

	FnKat::IntAttribute bucketOrderAttribute = imagineGSAttribute.getChildByName("bucket_order");
	unsigned int bucketOrder = 4;
	if (bucketOrderAttribute.isValid())
		bucketOrder = bucketOrderAttribute.getValue(4, false);

	FnKat::IntAttribute bucketSizeAttribute = imagineGSAttribute.getChildByName("bucket_size");
	unsigned int bucketSize = 48;
	if (bucketSizeAttribute.isValid())
		bucketSize = bucketSizeAttribute.getValue(48, false);

	FnKat::IntAttribute deterministicSamplesAttribute = imagineGSAttribute.getChildByName("deterministic_samples");
	int deterministicSamples = 1;
	if (deterministicSamplesAttribute.isValid())
		deterministicSamples = deterministicSamplesAttribute.getValue(1, false);

	FnKat::IntAttribute decomposeXFormsAttribute = imagineGSAttribute.getChildByName("decompose_xforms");
	if (decomposeXFormsAttribute.isValid())
		m_creationSettings.m_decomposeXForms = (decomposeXFormsAttribute.getValue(0, false) == 1);

	FnKat::IntAttribute discardGeometryAttribute = imagineGSAttribute.getChildByName("discard_geometry");
	if (discardGeometryAttribute.isValid())
		m_creationSettings.m_discardGeometry = (discardGeometryAttribute.getValue(0, false) == 1);

	//

	FnKat::IntAttribute textureCachingTypeAttribute = imagineGSAttribute.getChildByName("texture_caching_type");
	int textureCachingType = 1;
	if (textureCachingTypeAttribute.isValid())
		textureCachingType = textureCachingTypeAttribute.getValue(1, false);

	if (textureCachingType == 0)
	{
		GlobalContext::instance().setTextureCachingType(GlobalContext::eTextureCachingNone);
	}
	else
	{
		GlobalContext::instance().setTextureCachingType(textureCachingType == 1 ? GlobalContext::eTextureCachingLazyGlobal :
																				  GlobalContext::eTextureCachingLazyPerThread);

		FnKat::IntAttribute textureCacheMaxSizeAttribute = imagineGSAttribute.getChildByName("texture_cache_max_size");
		int textureCacheMaxSize = 4096;
		if (textureCacheMaxSizeAttribute.isValid())
			textureCacheMaxSize = textureCacheMaxSizeAttribute.getValue(4096, false);

		FnKat::IntAttribute textureCacheMaxOpenFileHandlesAttribute = imagineGSAttribute.getChildByName("texture_cache_max_file_handles");
		int textureCacheMaxOpenFileHandles = 744;
		if (textureCacheMaxOpenFileHandlesAttribute.isValid())
			textureCacheMaxOpenFileHandles = textureCacheMaxOpenFileHandlesAttribute.getValue(744, false);

		GlobalContext::instance().setTextureCacheMemoryLimit(textureCacheMaxSize);
		GlobalContext::instance().setTextureCacheFileHandleLimit(textureCacheMaxOpenFileHandles);

		FnKat::IntAttribute textureTileDataFixAttribute = imagineGSAttribute.getChildByName("texture_tile_data_fix");
		int textureTileDataFixType = 0;
		if (textureTileDataFixAttribute.isValid())
		{
			textureTileDataFixType = textureTileDataFixAttribute.getValue(0, false);
		}
		m_renderSettings.add("textureTileDataFix", textureTileDataFixType);

		FnKat::IntAttribute textureCacheCacheFileHandlesAttribute = imagineGSAttribute.getChildByName("texture_cache_cache_file_handles");
		int textureCacheCacheFileHandles = 1;
		if (textureCacheCacheFileHandlesAttribute.isValid())
			textureCacheCacheFileHandles = textureCacheCacheFileHandlesAttribute.getValue(1, false);

		if (textureCacheCacheFileHandles == 1)
		{
			m_renderSettings.add("useTextureFileHandleCaching", true);
		}

		int textureCacheDeleteTileItems = gsHelper.getIntParam("texture_delete_tile_items", 0);
		m_renderSettings.add("textureDeleteTileItems", (textureCacheDeleteTileItems == 1));

		FnKat::IntAttribute textureCacheGlobalMipmapBiasAttribute = imagineGSAttribute.getChildByName("texture_cache_global_mipmap_bias");
		int textureCacheGlobalMipmapBias = 0;
		if (textureCacheGlobalMipmapBiasAttribute.isValid())
			textureCacheGlobalMipmapBias = textureCacheGlobalMipmapBiasAttribute.getValue(0, false);

		if (textureCacheGlobalMipmapBias != 0)
		{
			m_renderSettings.add("textureGlobalMipmapBias", (float)textureCacheGlobalMipmapBias);
		}
	}

	//

	m_renderSettings.add("integrator", m_integratorType);
	m_renderSettings.add("mis", (bool)mis);
	m_renderSettings.add("evaluateGlossy", (bool)evaluateGlossy);
	m_renderSettings.add("allowIndirectCaustics", (bool)allowIndirectCaustics);
	m_renderSettings.add("russianRoulette", (bool)russianRoulette);

	if (useAdaptive == 1)
	{
		m_renderSettings.add("adaptive", true);
		m_renderSettings.add("adaptiveThreshold", adaptiveVarianceThreshold);
	}

	m_renderSettings.add("lightSamplingType", lightSamplingType);
	m_renderSettings.add("lightSamples", lightSamples);
	m_renderSettings.add("rayEpsilon", rayEpsilon);
	m_renderSettings.add("shadowRayEpsilon", shadowRayEpsilon);

	m_renderSettings.add("Iterations", (unsigned int)iterations);
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
	m_renderSettings.add("deterministicSamples", deterministicSamples);

	m_renderSettings.add("statsType", statisticsType);
	m_renderSettings.add("statsOutputType", statisticsOutputType);

	if (sceneAccelStructure == 1)
	{
		// set to BVHSSE
		m_pScene->getAccelerationStructureSettings().setType(2);
	}

	if (bakeDownScene == 1)
	{
		m_pScene->setBakeDownScene(true);
	}

	return true;
}

bool ImagineRender::configureDiskRenderOutputs(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
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

	m_pScene->setDefaultCamera(pRenderCamera);
}

void ImagineRender::buildSceneGeometry(Foundry::Katana::Render::RenderSettings& settings, FnKat::FnScenegraphIterator rootIterator)
{
	// force expand for the moment, instead of using lazy procedurals...

	SGLocationProcessor locProcessor(*m_pScene, m_creationSettings);

	locProcessor.processSGForceExpand(rootIterator);

	// add materials lazily
	std::vector<Material*> aMaterials;
	locProcessor.getFinalMaterials(aMaterials);

	MaterialManager& mm = m_pScene->getMaterialManager();

	mm.addMaterialsLazy(aMaterials);
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

	// if there's no light in the scene and we're not direct illumination, add a physical sky...
	if (m_pScene->getLightCount() == 0 && !(m_integratorType == 0 && m_ambientOcclusion))
	{
		PhysicalSky* pPhysicalSkyLight = new PhysicalSky();

		pPhysicalSkyLight->setRadius(1000.0f);

		m_pScene->addObject(pPhysicalSkyLight, false, false);
	}

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

	// if there's no light in the scene and we're not direct illumination, add a physical sky...
	if (m_pScene->getLightCount() == 0 && !(m_integratorType == 0 && m_ambientOcclusion))
	{
		PhysicalSky* pPhysicalSkyLight = new PhysicalSky();

		pPhysicalSkyLight->setRadius(1000.0f);

		m_pScene->addObject(pPhysicalSkyLight, false, false);
	}

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

	// if there's no light in the scene and we're not direct illumination, add a physical sky...
	if (m_pScene->getLightCount() == 0 && !(m_integratorType == 0 && m_ambientOcclusion))
	{
		PhysicalSky* pPhysicalSkyLight = new PhysicalSky();

		pPhysicalSkyLight->setRadius(1000.0f);

		m_pScene->addObject(pPhysicalSkyLight, false, false);
	}

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

#define SEND_ALL_FRAMES 1

bool ImagineRender::setupPreviewDataChannel(Foundry::Katana::Render::RenderSettings& settings)
{
	std::string katHost = getKatanaHost();

	size_t portSep = katHost.find(":");

	if (portSep == std::string::npos)
	{
		fprintf(stderr, "Error: Katana host and port not specified...\n");
		return false;
	}

	m_katanaHost = katHost.substr(0, portSep);

	std::string strPort = katHost.substr(portSep + 1);
	m_katanaPort = atol(strPort.c_str());

	// perversely, Katana requires the port number retrieved above to be incremented by 100,
	// despite the fact that KatanaPipe::connect() seems to handle doing that itself for the control
	// port (which is +100 the data port). Not doing this causes Katana to segfault on the connect()
	// call, leaving renderBoot orphaned doing the render in the background...
	m_katanaPort += 100;

	// customise channels we're going to do from settings...
	bool doExtaChannels = true;

	//
#if ENABLE_PREVIEW_RENDERS
	FnKat::Render::RenderSettings::ChannelBuffers interactiveRenderBuffers;
	settings.getChannelBuffers(interactiveRenderBuffers);

	FnKat::Render::RenderSettings::ChannelBuffers::const_iterator itBuffer = interactiveRenderBuffers.begin();
	for (; itBuffer != interactiveRenderBuffers.end(); ++itBuffer)
	{
		const FnKat::Render::RenderSettings::ChannelBuffer& buffer = (*itBuffer).second;

		const std::string& channelName = (*itBuffer).first;
		m_aInteractiveChannelNames.push_back(channelName);

		int frameID = atoi(buffer.bufferId.c_str());
		m_aInteractiveFrameIDs.push_back(frameID);

		std::string channelType = buffer.channelName;

		// detect extra AOVs...

		if (doExtaChannels && channelName == "n")
		{
			channelType = "rgb";
			m_aExtraInteractiveAOVs.push_back(RenderAOV(channelName, channelType, 3, frameID));
			m_renderSettings.add("output_normals", true);
			m_extraAOVsFlags |= COMPONENT_NORMAL;
		}
		else if (doExtaChannels && channelName == "wpp")
		{
			m_renderSettings.add("output_wpp", true);
			m_extraAOVsFlags |= COMPONENT_WPP;
		}

//		fprintf(stderr, "Buffers: %s: %i, %s\n", channelName.c_str(), frameID, channelType.c_str());
	}

	if (m_aInteractiveFrameIDs.empty())
	{
		fprintf(stderr, "Error: no channel buffers specified for interactive rendering...\n");
		return false;
	}

	// Katana expects the frame to be formatted like this, based on the ID pass frame...
	// Without doing this, it can't identify frames and channels...
	char szFN[128];
	sprintf(szFN, "id_katana_%i", m_aInteractiveFrameIDs[0]);
	std::string fFrameName(szFN);

	//

	if (m_pDataPipe)
	{
		delete m_pDataPipe;
		m_pDataPipe = NULL;
	}

	m_pDataPipe = FnKat::PipeSingleton::Instance(m_katanaHost, m_katanaPort);

	// try and connect...
	if (m_pDataPipe->connect() != 0)
	{
		fprintf(stderr, "Error: Can't connect to Katana data socket: %s:%u\n", m_katanaHost.c_str(), m_katanaPort);
		return false;
	}

	unsigned int originX = 0;
	unsigned int originY = 0;
/*
	if (m_ROIActive)
	{
		originX += m_ROIStartX;
		originY += m_ROIStartY;
	}
*/

	if (m_pFrame)
	{
		delete m_pFrame;
		m_pFrame = NULL;
	}

	m_pFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);

	int localFrameID = m_aInteractiveFrameIDs[0];

	// set the name
	std::string frameName;
	FnKat::encodeLegacyName(fFrameName, localFrameID, frameName);
	m_pFrame->setFrameName(frameName);

	m_pDataPipe->send(*m_pFrame);

	// build up channels we're going to do - always do the primary

	if (m_pPrimaryChannel)
	{
		delete m_pPrimaryChannel;
		m_pPrimaryChannel = NULL;
	}

	int channelID = 0;
	m_pPrimaryChannel = new FnKat::NewChannelMessage(*m_pFrame, channelID, m_renderHeight, m_renderWidth, originX, originY, 1.0f, 1.0f);

	std::string channelName;
	FnKat::encodeLegacyName(fFrameName, localFrameID, channelName);
	m_pPrimaryChannel->setChannelName(channelName);

	// total size of a pixel for a single channel (RGBA * float) = 16
	// this is used by Katana to work out how many Channels there are in the image...
	m_pPrimaryChannel->setDataSize(sizeof(float) * 4);

	m_pDataPipe->send(*m_pPrimaryChannel);

	// now do any extra AOVs...

	if (!m_aExtraInteractiveAOVs.empty())
	{
		std::vector<RenderAOV>::iterator itRenderAOV = m_aExtraInteractiveAOVs.begin();
		for (; itRenderAOV != m_aExtraInteractiveAOVs.end(); ++itRenderAOV)
		{
			RenderAOV& rAOV = *itRenderAOV;

#if SEND_ALL_FRAMES
			FnKat::NewFrameMessage* pLocalFrame = new FnKat::NewFrameMessage(getRenderTime(), m_renderHeight, m_renderWidth, originX, originY);

			localFrameID = rAOV.frameID;

			// set the name
			std::string frameName;
			FnKat::encodeLegacyName(fFrameName, localFrameID, frameName);
			pLocalFrame->setFrameName(frameName);

			m_pDataPipe->send(*pLocalFrame);
#endif

			rAOV.pChannelMessage = new FnKat::NewChannelMessage(*m_pFrame, ++channelID, m_renderHeight, m_renderWidth, originX, originY, 1.0f, 1.0f);

			FnKat::encodeLegacyName(rAOV.name, localFrameID, channelName);
			rAOV.pChannelMessage->setChannelName(channelName);

			// total size of a pixel for a single channel (numchannels * float)
			// this is used by Katana to work out how many channels there are in the image...
			rAOV.pChannelMessage->setDataSize(sizeof(float) * rAOV.numChannels);

			m_pDataPipe->send(*rAOV.pChannelMessage);
		}
	}
#endif

	return true;
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

	raytracer.renderScene(1.0f, NULL);

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

		raytracer.renderScene(1.0f, NULL);

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

		m_pRaytracer->renderScene(1.0f, &m_renderSettings);

		m_rendererOtherMemory = m_pRaytracer->getRendererMemoryUsage();
	}
#endif
}

void ImagineRender::renderFinished()
{
	// TODO: see if this is getting called on re-render...
#if ENABLE_PREVIEW_RENDERS
	bool doFinal = true;
	if (doFinal && m_pOutputImage)
	{
		// send through the entire image again...
		OutputImage imageCopy(*m_pOutputImage);

		const unsigned int origWidth = imageCopy.getWidth();
		const unsigned int origHeight = imageCopy.getHeight();

		if (m_integratorType != 0)
		{
			imageCopy.normaliseProgressive();
		}
		imageCopy.applyExposure(1.1f);

		FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(m_pPrimaryChannel));
		pNewTileMessage->setStartCoordinates(m_ROIStartX, m_ROIStartY);
		pNewTileMessage->setDataDimensions(origWidth, origHeight);

		unsigned int skipSize = 4 * sizeof(float);
		unsigned int dataSize = origWidth * origHeight * skipSize;
		unsigned char* pData = new unsigned char[dataSize];

		unsigned char* pDstRow = pData;

		for (unsigned int i = 0; i < origHeight; i++)
		{
			const Colour4f* pSrcRow = imageCopy.colourRowPtr(i);
			const Colour4f* pSrcPixel = pSrcRow;

			unsigned char* pDstPixel = pDstRow;

			for (unsigned int j = 0; j < origWidth; j++)
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

			pDstRow += origWidth * skipSize;
		}

		pNewTileMessage->setData(pData, dataSize);
		pNewTileMessage->setByteSkip(skipSize);

		m_pDataPipe->send(*pNewTileMessage);

		delete [] pData;
		pData = NULL;

		delete pNewTileMessage;

		m_pDataPipe->flushPipe(*m_pPrimaryChannel);
		m_pDataPipe->closeChannel(*m_pPrimaryChannel);
	}
#endif
	fprintf(stderr, "Render complete.\n");

	if (m_printMemoryStatistics != 0)
	{
		// run through all objects in the scene, building up geometry info

		GeometryInfo info;

		std::vector<Object*>::const_iterator itObject = m_pScene->renderableObjectItBegin();
		for (; itObject != m_pScene->renderableObjectItEnd(); ++itObject)
		{
			const Object* pObject = *itObject;

			pObject->fillGeometryInfo(info);
		}

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

#if ENABLE_PREVIEW_RENDERS
	if (m_pOutputImage)
	{
		delete m_pOutputImage;
		m_pOutputImage = NULL;
	}
#endif
}

bool ImagineRender::hasPendingDataUpdates() const
{
	return m_liveRenderState.hasUpdates();
}

int ImagineRender::applyPendingDataUpdates()
{
//	m_pRaytracer->terminate();

	m_liveRenderState.lock();

	std::vector<KatanaUpdateItem>::const_iterator itUpdate = m_liveRenderState.updatesBegin();
	for (; itUpdate != m_liveRenderState.updatesEnd(); ++itUpdate)
	{
		const KatanaUpdateItem& update = *itUpdate;

		if (update.camera)
		{
			// get hold of camera
			Camera* pCamera = m_pScene->getRenderCamera();

			pCamera->transform().setCachedMatrix(update.xform.data(), true);
		}
	}

	m_liveRenderState.unlock();

	m_liveRenderState.flushUpdates();

	restartLiveRender();

	return 0;
}

void ImagineRender::restartLiveRender()
{
	m_liveRenderState.lock();

	::usleep(500);

	m_pOutputImage->clearImage();

	if (m_pRaytracer)
	{
		delete m_pRaytracer;
		m_pRaytracer = NULL;
	}

	m_pRaytracer = new Raytracer(*m_pScene, m_renderThreads, true);
	m_pRaytracer->setExtraChannels(0);
	m_pRaytracer->setHost(this);

	m_pRaytracer->initialise(m_pOutputImage, m_renderSettings);

	m_liveRenderState.unlock();

	m_pRaytracer->renderScene(1.0f, &m_renderSettings);
}

int ImagineRender::queueDataUpdates(FnKat::GroupAttribute updateAttribute)
{
	m_liveRenderState.lock();
	if (!m_pRaytracer)
	{
		m_liveRenderState.unlock();
		return 0;
	}

	m_liveRenderState.unlock();

	unsigned int numItems = updateAttribute.getNumberOfChildren();

	for (unsigned int i = 0; i < numItems; i++)
	{
		FnKat::GroupAttribute dataUpdateItemAttribute = updateAttribute.getChildByIndex(i);

		if (!dataUpdateItemAttribute.isValid())
			continue;

		FnKat::StringAttribute typeAttribute = dataUpdateItemAttribute.getChildByName("type");

		if (!typeAttribute.isValid())
			continue;

		FnKat::StringAttribute locationAttribute = dataUpdateItemAttribute.getChildByName("location");
		FnKat::GroupAttribute attributesAttribute = dataUpdateItemAttribute.getChildByName("attributes");

		bool partialUpdate = false;

		FnKat::StringAttribute partialUpdateAttribute = attributesAttribute.getChildByName("partialUpdate");
		if (partialUpdateAttribute.isValid() && partialUpdateAttribute.getValue("", false) == "True")
		{
			partialUpdate = true;
		}

		std::string type = typeAttribute.getValue("", false);
		std::string location = locationAttribute.getValue("", false);

		if (location == m_renderCameraLocation)
		{
			FnKat::GroupAttribute xformAttribute = attributesAttribute.getChildByName("xform");

			if (xformAttribute.isValid())
			{
				// TODO: this seemingly needs to be done here... Need to work out why...
				m_liveRenderState.lock();
				if (m_pRaytracer)
				{
					m_pRaytracer->terminate();
				}
				m_liveRenderState.unlock();

				FnKat::DoubleAttribute xformMatrixAttribute = xformAttribute.getChildByName("global");
				if (!xformMatrixAttribute.isValid())
					continue;

				FnKat::DoubleConstVector matrixValues = xformMatrixAttribute.getNearestSample(0.0f);

				if (matrixValues.size() != 16)
					continue;

				KatanaUpdateItem newUpdate;
				newUpdate.camera = true;

				newUpdate.xform.resize(16);
				std::copy(matrixValues.begin(), matrixValues.end(), newUpdate.xform.begin());

				m_liveRenderState.addUpdate(newUpdate);
			}
		}
	}

	return 0;
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

void ImagineRender::tileDone(const TileInfo& tileInfo, unsigned int threadID)
{
	if (!m_pOutputImage)
		return;

	const unsigned int origWidth = m_pOutputImage->getWidth();
	const unsigned int origHeight = m_pOutputImage->getHeight();

	unsigned int x = tileInfo.x;
	unsigned int y = tileInfo.y;
	unsigned int width = tileInfo.width;
	unsigned int height = tileInfo.height;

	const bool doAprons = true;
	if (doAprons)
	{
		// add a pixel border around the output tile apron to account for pixel filters...
		const unsigned int tb = tileInfo.tileApronSize;
		if (x >= tb)
		{
			x -= tb;
			width += tb;
		}
		if (y >= tb)
		{
			y -= tb;
			height += tb;
		}
		if (x + width + tb < origWidth)
		{
			width += tb;
		}
		if (y + height + tb < origHeight)
		{
			height += tb;
		}
	}

	// the TileInfo x/y coordinates aren't in local subimage space for crop renders, so we need to offset them
	// back into the OutputImage's space
	unsigned int localSrcX = x;
	unsigned int localSrcY = y;

	if (m_ROIActive)
	{
		// need to clamp these for image apron borders for pixel filtering...
		if (m_ROIStartX > localSrcX)
		{
			localSrcX = 0;
		}
		else
		{
			localSrcX -= m_ROIStartX;
		}

		if (m_ROIStartY > localSrcY)
		{
			localSrcY = 0;
		}
		else
		{
			localSrcY -= m_ROIStartY;
		}
	}

	// take our own copy of a sub-set of the current final output image, containing just our tile area
	OutputImage imageCopy(*m_pOutputImage, localSrcX, localSrcY, width, height);
	if (m_integratorType != 0)
	{
		imageCopy.normaliseProgressive();
	}
	imageCopy.applyExposure(1.1f);

	FnKat::DataMessage* pNewTileMessage = new FnKat::DataMessage(*(m_pPrimaryChannel));

	if (!pNewTileMessage)
		return;

	pNewTileMessage->setStartCoordinates(x, y);

	pNewTileMessage->setDataDimensions(width, height);

	unsigned int skipSize = 4 * sizeof(float);
	unsigned int dataSize = width * height * skipSize;
	unsigned char* pData = new unsigned char[dataSize];

	unsigned char* pDstRow = pData;

	//
	x = 0;
	y = 0;

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

	delete [] pData;
	pData = NULL;

	delete pNewTileMessage;

	// now do any extra AOVS

	std::vector<RenderAOV>::iterator itRenderAOV = m_aExtraInteractiveAOVs.begin();
	for (; itRenderAOV != m_aExtraInteractiveAOVs.end(); ++itRenderAOV)
	{
		RenderAOV& rAOV = *itRenderAOV;

		pNewTileMessage = new FnKat::DataMessage(*(rAOV.pChannelMessage));

		if (!pNewTileMessage)
			continue;

		if (m_ROIActive)
		{
			// if ROI rendering is enabled, we annoyingly seem to have to handle offsetting this into the full
			// image format for Katana...
//			pNewTileMessage->setStartCoordinates(x + m_ROIStartX, y + m_ROIStartY);
			pNewTileMessage->setStartCoordinates(tileInfo.x, tileInfo.y);
		}
		else
		{
			pNewTileMessage->setStartCoordinates(tileInfo.x, tileInfo.y);
		}
		pNewTileMessage->setDataDimensions(width, height);

		unsigned int skipSize = rAOV.numChannels * sizeof(float);
		unsigned int dataSize = width * height * skipSize;
		unsigned char* pData = new unsigned char[dataSize];

		unsigned char* pDstRow = pData;

		//
		x = 0;
		y = 0;

		for (unsigned int i = 0; i < height; i++)
		{
			const Colour3f* pSrcRow = imageCopy.normalRowPtr(y + i);
			const Colour3f* pSrcPixel = pSrcRow + x;

			unsigned char* pDstPixel = pDstRow;

			for (unsigned int j = 0; j < width; j++)
			{
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

		delete [] pData;
		pData = NULL;

		delete pNewTileMessage;
	}
}

DEFINE_RENDER_PLUGIN(ImagineRender)

void registerPlugins()
{
	REGISTER_PLUGIN(ImagineRender, "imagine", 0, 1);
}
