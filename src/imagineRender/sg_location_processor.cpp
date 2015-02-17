#include "sg_location_processor.h"

#include <stdio.h>

#ifdef KAT_V_2
#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#include <FnGeolibServices/FnArbitraryOutputAttr.h>
#else
#include <RenderOutputUtils/RenderOutputUtils.h>
#endif

#include "katana_helpers.h"

#include "objects/mesh.h"
#include "objects/primitives/sphere.h"
#include "objects/compound_object.h"
#include "objects/compound_instance.h"
#include "objects/compact_mesh.h"

#include "geometry/compact_geometry_instance.h"

#include "lights/light.h"

SGLocationProcessor::SGLocationProcessor(Scene& scene, const CreationSettings& creationSettings) : m_scene(scene), m_creationSettings(creationSettings)
{
}

void SGLocationProcessor::processSG(FnKat::FnScenegraphIterator rootIterator)
{

}

void SGLocationProcessor::processSGForceExpand(FnKat::FnScenegraphIterator rootIterator)
{
	processLocationRecursive(rootIterator, 0);
}

void SGLocationProcessor::getFinalMaterials(std::vector<Material*>& aMaterials)
{
	aMaterials = m_materialHelper.getMaterialsVector();
}

void SGLocationProcessor::processLocationRecursive(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth)
{
	std::string type = iterator.getType();

	if (m_creationSettings.m_enableSubdivision)
	{
		if (type == "polymesh")
		{
			// TODO: use SG location delegates...
			processGeometryPolymeshCompact(iterator, false);
			return;
		}
		else if (type == "subdmesh")
		{
			processGeometryPolymeshCompact(iterator, true);
			return;
		}
	}
	else
	{
		if (type == "polymesh" || type == "subdmesh")
		{
			// TODO: use SG location delegates...
			processGeometryPolymeshCompact(iterator, false);
			return;
		}
	}
/*
	if (type[0] == 'i')
	{
		// excessive, but...
		const std::string& firstInstancePart = type.substr(1, 7);
		if (firstInstancePart == "nstance")
		{

		}
	}
*/
	if (type == "instance")
	{
		processInstance(iterator);
		return;
	}
	if (type == "instance source")
	{
		return;
	}
	if (m_creationSettings.m_specialiseAssemblies && type == "assembly")
	{
		processAssembly(iterator, currentDepth);
		return;
	}
	else if (/*type == "sphere" || */type == "nurbspatch") // hack, but works for now...
	{
		processSphere(iterator);
		return;
	}
	else if (type == "light")
	{
		// see if it's muted...
		FnKat::IntAttribute mutedAttribute = iterator.getAttribute("mute");
		if (mutedAttribute.isValid())
		{
			int mutedValue = mutedAttribute.getValue(0, false);
			if (mutedValue == 1)
				return;
		}

		processLight(iterator);
		return;
	}

	unsigned int nextDepth = currentDepth + 1;

	FnKat::FnScenegraphIterator child = iterator.getFirstChild();
	for (; child.isValid(); child = child.getNextSibling())
	{
		processLocationRecursive(child, nextDepth);
	}
}

#define FAST 0

void SGLocationProcessor::processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator, bool asSubD)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: polymesh '%s' does not have a 'geometry' attribute...\n", name.c_str());
		return;
	}

	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);

	// TODO: if we want to support facesets (modo's abc output annoyingly seems very pro-faceset) in the future, we're going
	//       to have to check here if there are any children of type faceset/polymesh below this iterator. If so, we'd
	//       need to ignore this location and just process the children.

	CompactGeometryInstance* pNewGeoInstance = createCompactGeometryInstanceFromLocation(iterator, asSubD, imagineStatements);
	if (!pNewGeoInstance)
	{
		return;
	}

	unsigned int customFlags = 0;

	if (m_creationSettings.m_triangleType == 1)
	{
		customFlags |= CompactGeometryInstance::CUST_GEO_TRIANGLE_COMPACT;
	}
	else
	{
		customFlags |= CompactGeometryInstance::CUST_GEO_TRIANGLE_FAST;
	}

	if (m_creationSettings.m_geoQuantisationType != 0)
	{
		if (m_creationSettings.m_geoQuantisationType == 2)
		{
			customFlags |= CompactGeometryInstance::CUST_GEO_ATTRIBUTE_QUANTISE_NORMAL_STANDARD;
			customFlags |= CompactGeometryInstance::CUST_GEO_ATTRIBUTE_QUANTISE_UV_STANDARD;
		}
		else if (m_creationSettings.m_geoQuantisationType == 1)
		{
			customFlags |= CompactGeometryInstance::CUST_GEO_ATTRIBUTE_QUANTISE_NORMAL_STANDARD;
		}
	}

	pNewGeoInstance->setCustomFlags(customFlags);

	CompactMesh* pNewMeshObject = new CompactMesh();

	pNewMeshObject->setCompactGeometryInstance(pNewGeoInstance);

	Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator);
	pNewMeshObject->setMaterial(pMaterial);

	if (!m_creationSettings.m_motionBlur)
	{
		// do transform
		FnKat::RenderOutputUtils::XFormMatrixVector xform = KatanaHelpers::getXFormMatrixStatic(iterator);

		const double* pMatrix = xform[0].getValues();
		pNewMeshObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
	}
	else
	{
		// see if we've got multiple xform samples
		FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixMB(iterator, true, m_creationSettings.m_shutterOpen,
																							 m_creationSettings.m_shutterClose);
		if (xforms.size() == 1)
		{
			// we haven't, so just assign transform normally...
			const double* pMatrix = xforms[0].getValues();
			pNewMeshObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
		}
		else
		{
			const double* pMatrix0 = xforms[0].getValues();
			const double* pMatrix1 = xforms[1].getValues();
			bool decompose = m_creationSettings.m_decomposeXForms;
			pNewMeshObject->transform().setAnimatedCachedMatrix(pMatrix0, pMatrix1, true, decompose); // invert the matrix for transpose
		}
	}

	processVisibilityAttributes(imagineStatements, pNewMeshObject);

	m_scene.addObject(pNewMeshObject, false, false);
}

void SGLocationProcessor::processAssembly(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth)
{
	CompoundObject* pCO = createCompoundObjectFromLocation(iterator, currentDepth);

	// do transform
	FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(iterator);
	const double* pMatrix = xforms[0].getValues();

	pCO->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pCO, false, false);
}

CompactGeometryInstance* SGLocationProcessor::createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD,
																						const FnKat::GroupAttribute& imagineStatements)
{
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		return NULL;
	}

	CompactGeometryInstance* pNewGeoInstance = new CompactGeometryInstance();

	std::vector<Point>& aPoints = pNewGeoInstance->getPoints();

	// copy across the points...

	FnKat::GroupAttribute pointAttribute = geometryAttribute.getChildByName("point");

	// linear list of components of Vec3 points
	FnKat::FloatAttribute pAttr = pointAttribute.getChildByName("P");

	unsigned int numPointTimeSamples = 1;

#ifdef KAT_V_2
	numPointTimeSamples = (unsigned int)pAttr.getNumberOfTimeSamples();
#else
	numPointTimeSamples = pAttr.getSampleTimes().size();
#endif

	if (!m_creationSettings.m_motionBlur || numPointTimeSamples <= 1)
	{
		FnKat::FloatConstVector sampleData = pAttr.getNearestSample(0.0f);

		unsigned int numItems = sampleData.size();

#if FAST
		aPoints.resize(numItems / 3);
		// convert to Point items
		unsigned int pointCount = 0;
		for (unsigned int i = 0; i < numItems; i += 3)
		{
			Point& point = aPoints[pointCount++];
			point.x = sampleData[i];
			point.y = sampleData[i + 1];
			point.z = sampleData[i + 2];
		}
#else
		aPoints.reserve(numItems / 3);
		// convert to Point items
		for (unsigned int i = 0; i < numItems; i += 3)
		{
			float x = sampleData[i];
			float y = sampleData[i + 1];
			float z = sampleData[i + 2];

			aPoints.push_back(Point(x, y, z));
		}
#endif
	}
	else
	{
		std::vector<float> aSampleTimes;
		KatanaHelpers::getRelevantSampleTimes(pAttr, aSampleTimes, m_creationSettings.m_shutterOpen, m_creationSettings.m_shutterClose);
		FnKat::FloatConstVector sampleData0 = pAttr.getNearestSample(aSampleTimes[0]);
		FnKat::FloatConstVector sampleData1 = pAttr.getNearestSample(aSampleTimes[aSampleTimes.size() - 1]);

		unsigned int numItems = sampleData0.size();

		aPoints.reserve((numItems / 3) * 2);
		// convert to Point items
		for (unsigned int i = 0; i < numItems; i += 3)
		{
			float x = sampleData0[i];
			float y = sampleData0[i + 1];
			float z = sampleData0[i + 2];

			aPoints.push_back(Point(x, y, z));

			// second sample

			x = sampleData1[i];
			y = sampleData1[i + 1];
			z = sampleData1[i + 2];

			aPoints.push_back(Point(x, y, z));
		}

		pNewGeoInstance->setTimeSamples(2);
	}

	// work out the faces...
	FnKat::GroupAttribute polyAttribute = geometryAttribute.getChildByName("poly");
	FnKat::IntAttribute polyStartIndexAttribute = polyAttribute.getChildByName("startIndex");
	FnKat::IntAttribute vertexListAttribute = polyAttribute.getChildByName("vertexList");

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	std::vector<uint32_t>& aPolyOffsets = pNewGeoInstance->getPolygonOffsets();

	unsigned int lastOffset = 0;

	aPolyOffsets.reserve(numFaces);

	// Imagine's CompactGeometryInstance assumes 0 is the first starting index, whereas Katana
	// specifies this and not the last one, so we need to ignore the first one, and add an extra on the end
	for (unsigned int i = 0; i < numFaces; i++)
	{
		unsigned int numVertices;
		if (i + 1 < numFaces)
		{
			numVertices = polyStartIndexAttributeValue[i + 1] - polyStartIndexAttributeValue[i];
		}
		else
		{
			// last one...
			numVertices = vertexListAttribute.getNumberOfTuples() - polyStartIndexAttributeValue[i];
		}

		unsigned int polyOffset = lastOffset + numVertices;
		aPolyOffsets.push_back(polyOffset);

		lastOffset += numVertices;
	}

	// we *could* just memcpy these across directly despite the differing sign type between Katana and Imagine,
	// as long as we're only using 31 bits of the value, but it's a bit hacky, so...
	std::vector<uint32_t>& aPolyIndices = pNewGeoInstance->getPolygonIndices();
	unsigned int numIndices = vertexListAttributeValue.size();
	aPolyIndices.resize(numIndices);
	for (unsigned int i = 0; i < numIndices; i++)
	{
		const int& value = vertexListAttributeValue[i];
		aPolyIndices[i] = (uint32_t)value;
	}

	unsigned int geoBuildFlags = GeometryInstance::GEO_BUILD_TESSELATE;

	if (asSubD)
	{
		geoBuildFlags |= GeometryInstance::GEO_BUILD_SUBDIVIDE;

		if (imagineStatements.isValid())
		{
			FnKat::IntAttribute subDivLevelsAttribute = imagineStatements.getChildByName("subdiv_levels");
			if (subDivLevelsAttribute.isValid())
			{
				unsigned int numSubdivLevels = subDivLevelsAttribute.getValue(1, false);
				pNewGeoInstance->setSubdivisionLevels(numSubdivLevels);
			}
		}

		// TODO: pull in crease values...
	}

	// see if we've got any Normals....
	FnKat::FloatAttribute normalsAttribute = iterator.getAttribute("geometry.vertex.N");
	// TODO: don't do for SubD's? Shouldn't have them anyway, but...
	if (m_creationSettings.m_useGeoNormals && normalsAttribute.isValid())
	{
		// check if the points had more than one time sample...
		if (!m_creationSettings.m_motionBlur || pNewGeoInstance->getTimeSamples() == 1)
		{
			FnKat::FloatConstVector normalsData = normalsAttribute.getNearestSample(0.0f);

			unsigned int numItems = normalsData.size();

			std::vector<Normal>& aNormals = pNewGeoInstance->getNormals();
#if FAST
			aNormals.resize(numItems / 3);
			// convert to Normal items
			unsigned int normalCount = 0;
			for (unsigned int i = 0; i < numItems; i += 3)
			{
				Normal& normal = aNormals[normalCount++];

				// need to reverse the normals as the winding order is opposite
				normal.x = -normalsData[i];
				normal.y = -normalsData[i + 1];
				normal.z = -normalsData[i + 2];
			}
#else
			aNormals.reserve(numItems / 3);
			// convert to Normal items
			for (unsigned int i = 0; i < numItems; i += 3)
			{
				float x = normalsData[i];
				float y = normalsData[i + 1];
				float z = normalsData[i + 2];

				// we need to reverse the normals as the winding order is opposite...
				aNormals.push_back(-Normal(x, y, z));
			}
#endif
		}
		else
		{
			std::vector<Normal>& aNormals = pNewGeoInstance->getNormals();

			std::vector<float> aSampleTimes;
			KatanaHelpers::getRelevantSampleTimes(normalsAttribute, aSampleTimes, m_creationSettings.m_shutterOpen, m_creationSettings.m_shutterClose);

			FnKat::FloatConstVector sampleData0 = normalsAttribute.getNearestSample(aSampleTimes[0]);
			FnKat::FloatConstVector sampleData1 = normalsAttribute.getNearestSample(aSampleTimes[aSampleTimes.size() - 1]);

			unsigned int numItems = sampleData0.size();

			aNormals.reserve((numItems / 3) * 2);
			// convert to Point items
			for (unsigned int i = 0; i < numItems; i += 3)
			{
				// first sample
				float x = sampleData0[i];
				float y = sampleData0[i + 1];
				float z = sampleData0[i + 2];

				// we need to reverse the normals as the winding order is opposite...
				aNormals.push_back(-Normal(x, y, z));

				// second sample
				x = sampleData1[i];
				y = sampleData1[i + 1];
				z = sampleData1[i + 2];

				// we need to reverse the normals as the winding order is opposite...
				aNormals.push_back(-Normal(x, y, z));
			}
		}
	}
	else
	{
		if (m_creationSettings.m_deduplicateVertexNormals)
		{
			geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_VERT_NORMALS_DD;
		}
		else
		{
			geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_VERT_NORMALS;
		}
	}

	FnKat::IntAttribute uvIndexAttribute;
	bool hasUVs = false;
	bool indexedUVs = false;
	// copy any UVs
	FnKat::GroupAttribute stAttribute = iterator.getAttribute("geometry.arbitrary.st", true);
	FnKat::FloatAttribute uvItemAttribute;
	if (stAttribute.isValid())
	{
#ifdef KAT_V_2
		FnKat::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);
#else
		FnKat::RenderOutputUtils::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);
#endif

		if (arbitraryAttribute.isValid())
		{
			if (arbitraryAttribute.hasIndexedValueAttr())
			{
				uvIndexAttribute = arbitraryAttribute.getIndexAttr(true);
				uvItemAttribute = arbitraryAttribute.getIndexedValueAttr();

				indexedUVs = true;
			}
			else
			{
				uvItemAttribute = arbitraryAttribute.getValueAttr();
				// we'll generate them later...
			}
		}
	}

	// we didn't find them in general place, so try and look for them in other locations...
	if (!uvItemAttribute.isValid())
	{
		uvItemAttribute = iterator.getAttribute("geometry.vertex.uv");

		if (!uvItemAttribute.isValid())
		{
			uvItemAttribute = iterator.getAttribute("geometry.point.uv");
		}
	}

	unsigned int numUVValues;

	if (uvItemAttribute.isValid())
	{
		hasUVs = true;
		std::vector<UV>& aUVs = pNewGeoInstance->getUVs();
		FnKat::FloatConstVector uvlist = uvItemAttribute.getNearestSample(0);
		numUVValues = processUVs(uvlist, aUVs);
	}

	// set any UV indices if necessary
	if (hasUVs)
	{
		// first of all, do a sanity check on whether the number of UVs is per vertex
		bool haveUVValuesForPerVertex = (numUVValues == aPolyIndices.size());

		// if so, we can possibly (optionally?) assume that we're not actually dealing with indexed UVs despite
		// what Katana tells us (at least in the case of the PrimitiveCreate's poly sphere and poly torus geometry)
		if (indexedUVs && haveUVValuesForPerVertex)
		{
			indexedUVs = false;
		}

		if (indexedUVs)
		{
			// if indexed, get hold the uv indices list
			FnKat::IntConstVector uvIndicesValue = uvIndexAttribute.getNearestSample(0.0f);

#if FAST
			// this isn't technically correct, but as long as we're only using 31 bits, will work...
			unsigned int numUVIndices = uvIndicesValue.size();
			uint32_t* pUVIndices = new uint32_t[numUVIndices];
			memcpy(pUVIndices, uvIndicesValue.data(), numUVIndices * sizeof(uint32_t));

			pNewGeoInstance->setUVIndicesRaw(pUVIndices, numUVIndices);
#else
			// we *could* just memcpy these across directly despite the differing sign type between Katana and Imagine,
			// as long as we're only using 31 bits of the value, but it's a bit hacky, so...
			unsigned int numUVIndices = uvIndicesValue.size();
			std::vector<uint32_t> aUVIndices;
			aUVIndices.resize(numUVIndices);
			for (unsigned int i = 0; i < numUVIndices; i++)
			{
				const int& value = uvIndicesValue[i];

				aUVIndices[i] = (uint32_t)value;
			}

			pNewGeoInstance->setUVIndices(aUVIndices);
#endif
		}
		else
		{
/*
			// just make a sequential list of UV indices - TODO: this is pretty silly having to do this: we should swap what no UV
			// indices means so that *this* is the default, which would use less memory (temporarily) and is much more common than
			// poly indices...
#if FAST
			uint32_t* pUVIndices = new uint32_t[numIndices];

			for (unsigned int i = 0; i < numIndices; i++)
			{
				pUVIndices[i] = i;
			}

			pNewGeoInstance->setUVIndicesRaw(pUVIndices, numIndices);
#else
			std::vector<uint32_t> aUVIndices;
			aUVIndices.resize(numIndices);

			for (unsigned int i = 0; i < numIndices; i++)
			{
				aUVIndices[i] = i;
			}

			pNewGeoInstance->setUVIndices(aUVIndices);
#endif

*/
		}

		// otherwise if we're not indexed, just set that we're using VertexUVs, and the overall vertex indices will be used.

		pNewGeoInstance->setHasPerVertexUVs(true);
	}

	FnKat::DoubleAttribute boundAttr = iterator.getAttribute("bound");
	if (boundAttr.isValid())
	{
		if (!m_creationSettings.m_motionBlur || pNewGeoInstance->getTimeSamples() == 1)
		{
			BoundaryBox bbox;
			FnKat::DoubleConstVector doubleValues = boundAttr.getNearestSample(0.0f);
			bbox.getMinimum() = Vector(doubleValues.at(0), doubleValues.at(2), doubleValues.at(4));
			bbox.getMaximum() = Vector(doubleValues.at(1), doubleValues.at(3), doubleValues.at(5));
			pNewGeoInstance->setBoundaryBox(bbox);
		}
		else
		{
			// if we've got motion blur, we've got a bit of a problem: we could get multiple time samples
			// for the bounds and union them, but for shutter angles clipped from the sample times, the
			// bounds would be too big making renders potentially much slower. We could do this shutter
			// angle interpolation/clamping work here (and it would be cheaper than getting Imagine to
			// brute force the bounds based on the point positions), but for the moment it's easier to
			// just get Imagine to do it, as at least we'll get tight accurate bboxes...

			geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_BBOX;
		}
	}
	else
	{
		// strictly-speaking, this means that the attributes are wrong, so we should probably ignore it, but
		// on the basis that we're force-expanding everything currently anyway...

		geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_BBOX;
	}

	// object settings...
	if (imagineStatements.isValid())
	{
		FnKat::FloatAttribute creaseAngleAttribute = imagineStatements.getChildByName("crease_angle");
		if (creaseAngleAttribute.isValid())
		{
			float creaseAngle = creaseAngleAttribute.getValue(0.8f, false);
			pNewGeoInstance->setCreaseAngle(creaseAngle);
		}
	}

	geoBuildFlags |= GeometryInstance::GEO_BUILD_FREE_SOURCE_DATA;

	pNewGeoInstance->setGeoBuildFlags(geoBuildFlags);

	return pNewGeoInstance;
}

CompoundObject* SGLocationProcessor::createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator, unsigned int baseLevelDepth)
{
	std::vector<Object*> aObjects;

	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);

	createCompoundObjectFromLocationRecursive(iterator, aObjects, baseLevelDepth, baseLevelDepth);

	CompoundObject* pNewCO = new CompoundObject();

	std::vector<Object*>::iterator itObject = aObjects.begin();
	for (; itObject != aObjects.end(); ++itObject)
	{
		Object* pObject = *itObject;

		pNewCO->addObject(pObject);
	}

	pNewCO->setType(CompoundObject::eBaked);

	unsigned char bakedFlags = m_creationSettings.m_specialisedTriangleType;
	pNewCO->setBakedFlags(bakedFlags);

	processVisibilityAttributes(imagineStatements, pNewCO);

	return pNewCO;
}

void SGLocationProcessor::createCompoundObjectFromLocationRecursive(FnKat::FnScenegraphIterator iterator, std::vector<Object*>& aObjects,
																	unsigned int baseLevelDepth, unsigned int currentDepth)
{
	std::string type = iterator.getType();

	bool isGeo = false;
	bool isSubD = false;

	if (type == "polymesh")
	{
		isGeo = true;
	}
	else if (type == "subdmesh")
	{
		isGeo = true;
		isSubD = m_creationSettings.m_enableSubdivision;
	}

	if (isGeo)
	{
		// get the geometry attributes group
		FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
		if (!geometryAttribute.isValid())
		{
			std::string name = iterator.getFullName();
			fprintf(stderr, "Warning: polymesh '%s' does not have a 'geometry' attribute...\n", name.c_str());
			return;
		}

		FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);

		CompactGeometryInstance* pNewGeoInstance = createCompactGeometryInstanceFromLocation(iterator, isSubD, imagineStatements);

		if (!pNewGeoInstance)
		{
			return;
		}

		CompactMesh* pNewMeshObject = new CompactMesh();

		pNewMeshObject->setCompactGeometryInstance(pNewGeoInstance);
		Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator);

		if (pMaterial)
		{
			pNewMeshObject->setMaterial(pMaterial);
		}
		else
		{
			// otherwise, set default
			pNewMeshObject->setDefaultMaterial();
		}

		// do transform - but limit the number of levels

		int depthLimit = currentDepth - baseLevelDepth;

		FnKat::GroupAttribute xformAttr = KatanaHelpers::buildLocationXformList(iterator, depthLimit);

		FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(xformAttr);

		const double* pMatrix = xforms[0].getValues();

		pNewMeshObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

		aObjects.push_back(pNewMeshObject);

		return;
	}

	unsigned int nextDepth = currentDepth + 1;

	FnKat::FnScenegraphIterator child = iterator.getFirstChild();
	for (; child.isValid(); child = child.getNextSibling())
	{
		createCompoundObjectFromLocationRecursive(child, aObjects, baseLevelDepth, nextDepth);
	}
}

void SGLocationProcessor::processInstance(FnKat::FnScenegraphIterator iterator)
{
	FnKat::StringAttribute instanceSourceAttribute = iterator.getAttribute("geometry.instanceSource");
	if (!instanceSourceAttribute.isValid())
	{
		return;
	}

	std::string instanceSourcePath = instanceSourceAttribute.getValue("", false);
	FnKat::FnScenegraphIterator itInstanceSource = iterator.getRoot().getByPath(instanceSourcePath);
	if (!itInstanceSource.isValid())
	{
		return;
	}

	bool isCompound = false;

	// see if we've created the source already...
	std::map<std::string, InstanceInfo>::const_iterator itFind = m_aInstances.find(instanceSourcePath);

	Object* pNewObject = NULL;

	if (itFind != m_aInstances.end())
	{
		// just create the item pointing to it

		const InstanceInfo& ii = (*itFind).second;

		if (ii.m_compound)
		{
			CompoundInstance* pNewCI = new CompoundInstance(ii.pCompoundObject);

			pNewObject = pNewCI;

			isCompound = true;
		}
		else
		{
			CompactMesh* pNewMesh = new CompactMesh();
			pNewMesh->setCompactGeometryInstance(static_cast<CompactGeometryInstance*>(ii.pGeoInstance));

			pNewObject = pNewMesh;
		}
	}
	else
	{
		bool isLeaf = !itInstanceSource.getFirstChild().isValid();
		// check two levels down, as that's more conventional...
		bool hasSubLeaf = isLeaf && (itInstanceSource.getFirstChild().getFirstChild().isValid());
		// if it's simply pointing to a mesh (so a leaf without a hierarchy)
		if (isLeaf)
		{
			// just build the source geometry and link to it....

			bool isSubD = m_creationSettings.m_enableSubdivision && itInstanceSource.getType() == "subdmesh";

			FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);

			CompactGeometryInstance* pNewInstance = createCompactGeometryInstanceFromLocation(itInstanceSource, isSubD, imagineStatements);

			InstanceInfo ii;
			ii.m_compound = false;
			ii.pGeoInstance = pNewInstance;

			m_aInstances[instanceSourcePath] = ii;

			CompactMesh* pNewMesh = new CompactMesh();
			pNewMesh->setCompactGeometryInstance(pNewInstance);

			pNewObject = pNewMesh;
		}
		else
		{
			// it's a full hierarchy, so we can specialise by building a compound object containing all the sub-objects

			// -1 isn't right, but it works due to the fact that it effectively strips off the base level transform, which
			// is what we want...
			CompoundObject* pCO = createCompoundObjectFromLocation(itInstanceSource, -1);

			// this one is set to be hidden, but we need to add it to the scene...

			InstanceInfo ii;
			ii.m_compound = true;

			ii.pCompoundObject = pCO;

			m_aInstances[instanceSourcePath] = ii;

			pNewObject = pCO;

			isCompound = true;
		}
	}

	if (!pNewObject)
		return;

	//

	// if it's not a compound object, we can assign a material to it, so look for one...
	if (!isCompound)
	{
		Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator);
		pNewObject->setMaterial(pMaterial);
	}

	// do transform

	if (!m_creationSettings.m_motionBlur)
	{
		// do transform
		FnKat::RenderOutputUtils::XFormMatrixVector xform = KatanaHelpers::getXFormMatrixStatic(iterator);

		const double* pMatrix = xform[0].getValues();
		pNewObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
	}
	else
	{
		// see if we've got multiple xform samples
		FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixMB(iterator, true, m_creationSettings.m_shutterOpen,
																							 m_creationSettings.m_shutterClose);
		if (xforms.size() == 1)
		{
			// we haven't, so just assign transform normally...
			const double* pMatrix = xforms[0].getValues();
			pNewObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
		}
		else
		{
			const double* pMatrix0 = xforms[0].getValues();
			const double* pMatrix1 = xforms[1].getValues();
			bool decompose = m_creationSettings.m_decomposeXForms;
			pNewObject->transform().setAnimatedCachedMatrix(pMatrix0, pMatrix1, true, decompose); // invert the matrix for transpose
		}
	}

	m_scene.addObject(pNewObject, false, false);
}

void SGLocationProcessor::processSphere(FnKat::FnScenegraphIterator iterator)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: sphere: %s does not have a geometry attribute...\n", name.c_str());
		return;
	}

	float radius = 1.0f;

	FnKat::DoubleAttribute radiusAttr = geometryAttribute.getChildByName("radius");
	if (radiusAttr.isValid())
	{
		radius = radiusAttr.getValue(1.0f, false);
	}

	Sphere* pSphere = new Sphere((float)radius, 24);

	Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator);
	pSphere->setMaterial(pMaterial);

	// do transform

	if (!m_creationSettings.m_motionBlur)
	{
		// do transform
		FnKat::RenderOutputUtils::XFormMatrixVector xform = KatanaHelpers::getXFormMatrixStatic(iterator);

		const double* pMatrix = xform[0].getValues();
		pSphere->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
	}
	else
	{
		// see if we've got multiple xform samples
		FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixMB(iterator, true, m_creationSettings.m_shutterOpen,
																							 m_creationSettings.m_shutterClose);
		if (xforms.size() == 1)
		{
			// we haven't, so just assign transform normally...
			const double* pMatrix = xforms[0].getValues();
			pSphere->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
		}
		else
		{
			const double* pMatrix0 = xforms[0].getValues();
			const double* pMatrix1 = xforms[1].getValues();
			bool decompose = m_creationSettings.m_decomposeXForms;
			pSphere->transform().setAnimatedCachedMatrix(pMatrix0, pMatrix1, true, decompose); // invert the matrix for transpose
		}
	}

	m_scene.addObject(pSphere, false, false);
}

void SGLocationProcessor::processLight(FnKat::FnScenegraphIterator iterator)
{
	FnKat::GroupAttribute lightMaterialAttrib = m_materialHelper.getMaterialForLocation(iterator);

	Light* pNewLight = m_lightHelper.createLight(lightMaterialAttrib);

	if (!pNewLight)
		return;

	FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(iterator);
	const double* pMatrix = xforms[0].getValues();

	pNewLight->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pNewLight, false, false);
}

void SGLocationProcessor::processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Object* pObject)
{
	if (!imagineStatements.isValid())
		return;

	FnKat::GroupAttribute visibilityAttributes = imagineStatements.getChildByName("visibility");
	if (visibilityAttributes.isValid())
	{
		int cameraVis = 1;
		int shadowVis = 1;
		int diffuseVis = 1;
		int glossyVis = 1;
		int reflectionVis = 1;
		int refractionVis = 1;

		KatanaAttributeHelper helper(visibilityAttributes);

		cameraVis = helper.getIntParam("camera", 1);
		shadowVis = helper.getIntParam("shadow", 1);
		diffuseVis = helper.getIntParam("diffuse", 1);
		glossyVis = helper.getIntParam("glossy", 1);
		reflectionVis = helper.getIntParam("reflection", 1);
		refractionVis = helper.getIntParam("refraction", 1);

		unsigned char finalVisibility = 0;

		if (cameraVis)
			finalVisibility |= RENDER_VISIBILITY_CAMERA;
		if (shadowVis)
			finalVisibility |= RENDER_VISIBILITY_SHADOW;
		if (diffuseVis)
			finalVisibility |= RENDER_VISIBILITY_DIFFUSE;
		if (glossyVis)
			finalVisibility |= RENDER_VISIBILITY_GLOSSY;
		if (reflectionVis)
			finalVisibility |= RENDER_VISIBILITY_REFLECTION;
		if (refractionVis)
			finalVisibility |= RENDER_VISIBILITY_REFRACTION;

		pObject->setRenderVisibilityFlags(finalVisibility);
	}
}

unsigned int SGLocationProcessor::processUVs(FnKat::FloatConstVector& uvlist, std::vector<UV>& aUVs)
{
	unsigned int numItems = uvlist.size();

	unsigned int numUVValues = numItems / 2;

	aUVs.resize(numUVValues);

	std::vector<UV>::iterator itUV = aUVs.begin();

	if (m_creationSettings.m_flipT == 0) // do nothing
	{
		for (unsigned int i = 0; i < numItems; i += 2)
		{
			UV& uv = *itUV++;
			uv.u = uvlist[i];
			uv.v = uvlist[i + 1];
		}
	}
	else if (m_creationSettings.m_flipT == 1)
	{
		// fully flip t
		for (unsigned int i = 0; i < numItems; i += 2)
		{
			UV& uv = *itUV++;
			uv.u = uvlist[i];
			uv.v = 1.0 - uvlist[i + 1];
		}
	}
	else
	{
		// flip the t value over within the UDIM tile space.
		for (unsigned int i = 0; i < numItems; i += 2)
		{
			UV& uv = *itUV++;
			uv.u = uvlist[i];
			float tempV = uvlist[i + 1];

			float majorValue = 0.0f;
			float minorValue = modff(tempV, &majorValue);

			float finalValue = 1.0f - minorValue;
			finalValue += majorValue;

			uv.v = finalValue;
		}
	}

	return numUVValues;
}
