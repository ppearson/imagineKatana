#include "imagine_render.h"

#include "utilities.h"

#include "katana_helpers.h"
#include "imagine_utils.h"

// imagine stuff
#include "global_context.h"
#include "output_context.h"

using namespace Imagine;

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

	float adaptiveVarianceThreshold = gsHelper.getFloatParam("adaptive_variance_threshold", 0.05f);

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
	float filterScale = gsHelper.getFloatParam("filter_scale", 1.0f);

	unsigned int samplerType = gsHelper.getIntParam("sampler_type", 1);
	m_renderSettings.add("sampler_type", samplerType);
	
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
	
	FnKat::IntAttribute specialisedDetectInstancesAttribute = imagineGSAttribute.getChildByName("specialise_detect_instances");
	m_creationSettings.m_specialisedDetectInstances = true;
	if (specialisedDetectInstancesAttribute.isValid())
	{
		m_creationSettings.m_specialisedDetectInstances = (specialisedDetectInstancesAttribute.getValue(1, false) == 1);
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
	m_printMemoryStatistics = 0;
	if (printMemoryStatisticsAttribute.isValid())
		m_printMemoryStatistics = printMemoryStatisticsAttribute.getValue(0, false);

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
		int textureCacheCacheFileHandles = 0;
		if (textureCacheCacheFileHandlesAttribute.isValid())
			textureCacheCacheFileHandles = textureCacheCacheFileHandlesAttribute.getValue(0, false);

		if (textureCacheCacheFileHandles == 1)
		{
			m_renderSettings.add("useTextureFileHandleCaching", true);
		}
		else
		{
			m_renderSettings.add("useTextureFileHandleCaching", false);
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
	m_renderSettings.add("filter_scale", filterScale);

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
		
		unsigned int flags = 0;
		
		unsigned int triangleType = m_creationSettings.m_specialisedTriangleType;
		
		// first 3 bytes are triangle type		
		flags = triangleType;
		
		if (m_creationSettings.m_geoQuantisationType != 0)
		{
			flags |= GEO_QUANTISED;
		}
		
		if (m_creationSettings.m_specialisedDetectInstances)
		{
			flags |= USE_INSTANCES;
		}
		
		m_pScene->setGeometryBakingFlags(flags);
	}

	return true;
}
