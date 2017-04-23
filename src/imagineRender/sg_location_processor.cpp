#include "sg_location_processor.h"

#include <stdio.h>

#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#include <FnGeolibServices/FnArbitraryOutputAttr.h>

#include "katana_helpers.h"
#include "id_state.h"
#include "imagine_utils.h"

#include "objects/mesh.h"
#include "objects/primitives/sphere.h"
#include "objects/compound_object.h"
#include "objects/compound_instance.h"
#include "objects/compact_mesh.h"

#include "geometry/compact_geometry_instance.h"

#include "lights/light.h"


using namespace Imagine;

SGLocationProcessor::SGLocationProcessor(Scene& scene, const CreationSettings& creationSettings, IDState* pIDState)
	: m_scene(scene), m_creationSettings(creationSettings), m_pIDState(pIDState),
											m_isLiveRender(false)
{
}

SGLocationProcessor::~SGLocationProcessor()
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

void SGLocationProcessor::addObjectToScene(Object* pObject, FnKat::FnScenegraphIterator sgIterator)
{
	if (m_isLiveRender)
	{
		// if we're doing live rendering, we want location names to be assigned to objects and Imagine to build up
		// a lookup table
		pObject->setName(sgIterator.getFullName(), false);
	}

	m_scene.addObjectEmbedded(pObject, m_isLiveRender);
}

// the assumption is these will be unique - this is only really being done for stats purposes currently, so is optional,
// but it could be used to make use of Imagine's ability to automatically de-duplicate/instance geo, but that's not currently
// worth it with the way instances should be done in Katana, so...
void SGLocationProcessor::registerGeometryInstance(Imagine::GeometryInstance* pGeoInstance)
{
	m_scene.getGeometryManager().addRawGeometryInstance(pGeoInstance);
}

void SGLocationProcessor::processLocationRecursive(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth)
{
	std::string type = iterator.getType();

//	std::string fullName = iterator.getFullName();

//	fprintf(stderr, "location: %s, type: %s\n", fullName.c_str(), type.c_str());

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

	if (type == "instance")
	{
		processInstance(iterator);
		return;
	}
	if (type == "instance array")
	{
		processInstanceArray(iterator);
		return;
	}
	if (type == "instance source")
	{
		return;
	}

	if (m_creationSettings.m_specialiseType == CreationSettings::eAssembly && type == "assembly")
	{
		processSpecialisedType(iterator, currentDepth);
		return;
	}
	if (m_creationSettings.m_specialiseType == CreationSettings::eComponent && type == "component")
	{
		processSpecialisedType(iterator, currentDepth);
		return;
	}
	else if (type == "sphere" || type == "nurbspatch") // hack, but works for now...
	{
		processSphere(iterator);
		return;
	}
	else if (type == "light")
	{
		// see if it's muted...
		FnKat::IntAttribute mutedAttribute = iterator.getAttribute("mute", true);
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

	const bool evictChildTraversal = true;

	FnKat::FnScenegraphIterator child = iterator.getFirstChild(evictChildTraversal);
	// evict so potentially Katana can free up memory that we've already processed.
	for (; child.isValid(); child = child.getNextSibling(true))
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

	CompactGeometryInstance* pNewGeoInstance = NULL;
	if (!m_creationSettings.m_discardGeometry)
	{
		pNewGeoInstance = createCompactGeometryInstanceFromLocation(iterator, asSubD, imagineStatements);
	}
	else
	{
		pNewGeoInstance = createCompactGeometryInstanceFromLocationDiscard(iterator, asSubD, imagineStatements);
	}

	if (!pNewGeoInstance)
	{
		return;
	}

	unsigned int customFlags = getCustomGeoFlags();
	pNewGeoInstance->setCustomFlags(customFlags);

	CompactMesh* pNewMeshObject = new CompactMesh();

	pNewMeshObject->setCompactGeometryInstance(pNewGeoInstance);
	registerGeometryInstance(pNewGeoInstance);

	Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator, imagineStatements);
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

	if (m_pIDState)
	{
		unsigned int objectID = sendObjectID(iterator);
		pNewMeshObject->setObjectID(objectID);
	}

	addObjectToScene(pNewMeshObject, iterator);
}

void SGLocationProcessor::processSpecialisedType(FnKat::FnScenegraphIterator iterator, unsigned int currentDepth)
{
	CompoundObject* pCO = createCompoundObjectFromLocation(iterator, currentDepth);

	if (!pCO)
	{
		return;
	}

	if (!m_creationSettings.m_motionBlur)
	{
		// do transform
		FnKat::RenderOutputUtils::XFormMatrixVector xform = KatanaHelpers::getXFormMatrixStatic(iterator);

		const double* pMatrix = xform[0].getValues();
		pCO->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
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
			pCO->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose
		}
		else
		{
			const double* pMatrix0 = xforms[0].getValues();
			const double* pMatrix1 = xforms[1].getValues();
			bool decompose = m_creationSettings.m_decomposeXForms;
			pCO->transform().setAnimatedCachedMatrix(pMatrix0, pMatrix1, true, decompose); // invert the matrix for transpose
		}
	}

	addObjectToScene(pCO, iterator);
}

CompactGeometryInstance* SGLocationProcessor::createCompactGeometryInstanceFromLocation(FnKat::FnScenegraphIterator iterator, bool asSubD,
																						const FnKat::GroupAttribute& imagineStatements)
{
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		return NULL;
	}

	// default setting is false from the user's perspective, but in reality, we do the opposite
	bool flipFaces = false;

	FnKat::FloatAttribute creaseAngleAttribute;
	// object settings...
	if (imagineStatements.isValid())
	{
		creaseAngleAttribute = imagineStatements.getChildByName("crease_angle");
		FnKat::IntAttribute flipFacesAttribute = imagineStatements.getChildByName("flip_faces");
		if (flipFacesAttribute.isValid())
		{
			flipFaces = flipFacesAttribute.getValue(0, false);
		}
	}
	
	// invert flip to actually do the correct logic from Imagine's point-of-view to convert the faces
	// to native Imagine winding order...
	flipFaces = !flipFaces;

	CompactGeometryInstance* pNewGeoInstance = new CompactGeometryInstance();

	std::vector<Point>& aPoints = pNewGeoInstance->getPoints();

	// copy across the points...

	FnKat::GroupAttribute pointAttribute = geometryAttribute.getChildByName("point");

	// linear list of components of Vec3 points
	FnKat::FloatAttribute pAttr = pointAttribute.getChildByName("P");

	unsigned int numPointTimeSamples = 1;
	numPointTimeSamples = (unsigned int)pAttr.getNumberOfTimeSamples();

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

	// guard against bad data we sometime get from .abc files
	if (!polyStartIndexAttribute.isValid() || polyStartIndexAttribute.getNumberOfTuples() == 0)
	{
		if (pNewGeoInstance)
		{
			delete pNewGeoInstance;
			return NULL;
		}
	}

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	std::vector<uint32_t>& aPolyOffsets = pNewGeoInstance->getPolygonOffsets();
	aPolyOffsets.reserve(numFaces);

	unsigned int numIndices = vertexListAttributeValue.size();
	std::vector<uint32_t>& aPolyIndices = pNewGeoInstance->getPolygonIndices();
	aPolyIndices.resize(numIndices);

	if (!flipFaces)
	{
		unsigned int lastOffset = 0;

		// Imagine's CompactGeometryInstance assumes 0 is the first starting index, whereas Katana
		// specifies the first one and the last one, so we need to ignore the first one.
		// Note: Although the assumption here is that the first index *is* actually 0, which in certain
		//       cases it might not be - e.g. re-writing attributes to cull faces. But generally
		//       this does work in practice.
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

				// TODO: this isn't strictly-speaking safe - the if startIndex array is missing the last
				//       item, but vertexList still has the correct number of items, then numVertices
				//       ends up being bigger than it should be. But for correct geometry attributes
				//       this does work.
				numVertices = vertexListAttribute.getNumberOfTuples() - polyStartIndexAttributeValue[i];
			}

			unsigned int polyOffset = lastOffset + numVertices;
			aPolyOffsets.push_back(polyOffset);

			lastOffset += numVertices;
		}

		for (unsigned int i = 0; i < numIndices; i++)
		{
			const int& value = vertexListAttributeValue[i];
			aPolyIndices[i] = (uint32_t)value;
		}
	}
	else
	{
		// do them backwards, to reverse the faces

		unsigned int lastOffset = 0;
		unsigned int polyIndexCounter = 0;

		// because of Katana's use of the first AND the last indices, we can skip the last one, as the last one (original first) should be 0 (but it's not guarenteed to be!)...
		for (int i = numFaces; i > 0; i--)
		{
			int startIndex = polyStartIndexAttributeValue[i];
			int endIndex = polyStartIndexAttributeValue[i - 1];

			int numVertices = startIndex - endIndex;

			int polyOffset = lastOffset + numVertices;
			aPolyOffsets.push_back(polyOffset);

			lastOffset += numVertices;

			// now copy the indices within this loop, walking backwards from startIndex (which should be bigger) to endIndex
			for (int j = startIndex; j > endIndex; j--)
			{
				const int& value = vertexListAttributeValue[j - 1];
				aPolyIndices[polyIndexCounter++] = (uint32_t)value;
			}
		}
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
	if (m_creationSettings.m_useGeoNormals && normalsAttribute.isValid() && !asSubD)
	{
		// check if the points had more than one time sample...
		if (!m_creationSettings.m_motionBlur || pNewGeoInstance->getTimeSamples() == 1)
		{
			FnKat::FloatConstVector normalsData = normalsAttribute.getNearestSample(0.0f);

			unsigned int numItems = normalsData.size();

			std::vector<Normal>& aNormals = pNewGeoInstance->getNormals();

			if (!flipFaces)
			{
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
#if FAST
				aNormals.resize(numItems / 3);
				// convert to Normal items
				unsigned int normalCount = 0;
				for (unsigned int i = 0; i < numItems; i += 3)
				{
					Normal& normal = aNormals[normalCount++];

					normal.x = normalsData[i];
					normal.y = normalsData[i + 1];
					normal.z = normalsData[i + 2];
				}
#else
				aNormals.reserve(numItems / 3);
				// convert to Normal items
				for (unsigned int i = 0; i < numItems; i += 3)
				{
					float x = normalsData[i];
					float y = normalsData[i + 1];
					float z = normalsData[i + 2];

					aNormals.push_back(Normal(x, y, z));
				}
#endif
			}
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
		FnKat::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);

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
//			indexedUVs = false;
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
	if (m_creationSettings.m_useBounds && boundAttr.isValid())
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

	if (creaseAngleAttribute.isValid())
	{
		float creaseAngle = creaseAngleAttribute.getValue(0.8f, false);
		pNewGeoInstance->setCreaseAngle(creaseAngle);
	}

	geoBuildFlags |= GeometryInstance::GEO_BUILD_FREE_SOURCE_DATA;

	pNewGeoInstance->setGeoBuildFlags(geoBuildFlags);

	return pNewGeoInstance;
}

CompactGeometryInstance* SGLocationProcessor::createCompactGeometryInstanceFromLocationDiscard(FnKat::FnScenegraphIterator iterator, bool asSubD,
																						const FnKat::GroupAttribute& imagineStatements)
{
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		return NULL;
	}

	FnKat::GroupAttribute pointAttribute = geometryAttribute.getChildByName("point");

	// linear list of components of Vec3 points
	FnKat::FloatAttribute pAttr = pointAttribute.getChildByName("P");

	unsigned int numPointTimeSamples = 1;
	numPointTimeSamples = (unsigned int)pAttr.getNumberOfTimeSamples();

	if (!m_creationSettings.m_motionBlur || numPointTimeSamples <= 1)
	{
		FnKat::FloatConstVector sampleData = pAttr.getNearestSample(0.0f);

		unsigned int numItems = sampleData.size();
	}
	else
	{
		std::vector<float> aSampleTimes;
		KatanaHelpers::getRelevantSampleTimes(pAttr, aSampleTimes, m_creationSettings.m_shutterOpen, m_creationSettings.m_shutterClose);
		FnKat::FloatConstVector sampleData0 = pAttr.getNearestSample(aSampleTimes[0]);
		FnKat::FloatConstVector sampleData1 = pAttr.getNearestSample(aSampleTimes[aSampleTimes.size() - 1]);

		unsigned int numItems = sampleData0.size();
	}

	// work out the faces...
	FnKat::GroupAttribute polyAttribute = geometryAttribute.getChildByName("poly");
	FnKat::IntAttribute polyStartIndexAttribute = polyAttribute.getChildByName("startIndex");
	FnKat::IntAttribute vertexListAttribute = polyAttribute.getChildByName("vertexList");

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	unsigned int numIndices = polyStartIndexAttributeValue.size();

	// see if we've got any Normals....
	FnKat::FloatAttribute normalsAttribute = iterator.getAttribute("geometry.vertex.N");
	// TODO: don't do for SubD's? Shouldn't have them anyway, but...
	if (m_creationSettings.m_useGeoNormals && normalsAttribute.isValid())
	{
		// check if the points had more than one time sample...
		if (!m_creationSettings.m_motionBlur)
		{
			FnKat::FloatConstVector normalsData = normalsAttribute.getNearestSample(0.0f);

			unsigned int numItems = normalsData.size();
		}
		else
		{
			std::vector<float> aSampleTimes;
			KatanaHelpers::getRelevantSampleTimes(normalsAttribute, aSampleTimes, m_creationSettings.m_shutterOpen, m_creationSettings.m_shutterClose);

			FnKat::FloatConstVector sampleData0 = normalsAttribute.getNearestSample(aSampleTimes[0]);
			FnKat::FloatConstVector sampleData1 = normalsAttribute.getNearestSample(aSampleTimes[aSampleTimes.size() - 1]);

			unsigned int numItems = sampleData0.size();
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
		FnKat::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);

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
		FnKat::FloatConstVector uvlist = uvItemAttribute.getNearestSample(0);
	}

	// set any UV indices if necessary
	if (hasUVs)
	{
		if (indexedUVs)
		{
			// if indexed, get hold the uv indices list
			FnKat::IntConstVector uvIndicesValue = uvIndexAttribute.getNearestSample(0.0f);

		}
	}

	FnKat::DoubleAttribute boundAttr = iterator.getAttribute("bound");
	if (m_creationSettings.m_useBounds && boundAttr.isValid())
	{
		if (!m_creationSettings.m_motionBlur)
		{
			BoundaryBox bbox;
			FnKat::DoubleConstVector doubleValues = boundAttr.getNearestSample(0.0f);
			bbox.getMinimum() = Vector(doubleValues.at(0), doubleValues.at(2), doubleValues.at(4));
			bbox.getMaximum() = Vector(doubleValues.at(1), doubleValues.at(3), doubleValues.at(5));
		}
	}

	return NULL;
}

CompoundObject* SGLocationProcessor::createCompoundObjectFromLocation(FnKat::FnScenegraphIterator iterator, unsigned int baseLevelDepth)
{
	std::vector<Object*> aObjects;

	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);

	createCompoundObjectFromLocationRecursive(iterator, aObjects, baseLevelDepth, baseLevelDepth);

	if (aObjects.empty())
		return NULL;

	CompoundObject* pNewCO = new CompoundObject();

	std::vector<Object*>::iterator itObject = aObjects.begin();
	for (; itObject != aObjects.end(); ++itObject)
	{
		Object* pObject = *itObject;

		pNewCO->addObject(pObject);
	}

	pNewCO->setType(CompoundObject::eBaked);

	unsigned char bakedFlags = m_creationSettings.m_specialisedTriangleType;

	if (m_creationSettings.m_geoQuantisationType != 0)
	{
		bakedFlags |= GEO_QUANTISED;
	}
	
	if (m_creationSettings.m_specialisedDetectInstances)
	{
		bakedFlags |= USE_INSTANCES;
	}
	
	if (m_creationSettings.m_chunkedParallelBuild)
	{
		bakedFlags |= CHUNKED_PARALLEL_BUILD;
	}

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
	bool isInstance = false;

	if (type == "polymesh")
	{
		isGeo = true;
	}
	else if (type == "subdmesh")
	{
		isGeo = true;
		isSubD = m_creationSettings.m_enableSubdivision;
	}
	else if (type == "instance")
	{
		isInstance = true;
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

		CompactGeometryInstance* pNewGeoInstance = NULL;

		if (!m_creationSettings.m_discardGeometry)
		{
			pNewGeoInstance = createCompactGeometryInstanceFromLocation(iterator, isSubD, imagineStatements);
		}
		else
		{
			pNewGeoInstance = createCompactGeometryInstanceFromLocationDiscard(iterator, isSubD, imagineStatements);
		}

		if (!pNewGeoInstance)
		{
			return;
		}

		CompactMesh* pNewMeshObject = new CompactMesh();

		pNewMeshObject->setCompactGeometryInstance(pNewGeoInstance);
		registerGeometryInstance(pNewGeoInstance);

		Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator, imagineStatements);

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
	else if (isInstance)
	{
		FnKat::StringAttribute instanceSourceAttribute = iterator.getAttribute("geometry.instanceSource");
		if (!instanceSourceAttribute.isValid())
		{
			// we can't do anything...
			return;
		}

		std::string instanceSourcePath = instanceSourceAttribute.getValue("", false);
		if (instanceSourcePath.empty())
		{
			// we can't do anything
			return;
		}

		InstanceInfo instanceInfo = findOrBuildInstanceSourceItem(iterator, instanceSourcePath);
		if (!instanceInfo.pCompoundObject && !instanceInfo.pGeoInstance)
		{
			// we can't do anything
			return;
		}

		return;
	}

	unsigned int nextDepth = currentDepth + 1;

	FnKat::FnScenegraphIterator child = iterator.getFirstChild();
	for (; child.isValid(); child = child.getNextSibling())
	{
		createCompoundObjectFromLocationRecursive(child, aObjects, baseLevelDepth, nextDepth);
	}
}

// this builds and adds to the instance lookup map any instance locations at the instance source path
SGLocationProcessor::InstanceInfo SGLocationProcessor::findOrBuildInstanceSourceItem(FnKat::FnScenegraphIterator iterator, const std::string& instanceSourcePath)
{
	InstanceInfo nullInfo; // NULL by default

	// see if we've created the source already...
	std::map<std::string, InstanceInfo>::const_iterator itFind = m_aInstances.find(instanceSourcePath);

	if (itFind != m_aInstances.end())
	{
		// just create the item pointing to it

		const InstanceInfo& ii = (*itFind).second;

		if (!ii.pCompoundObject && !ii.pGeoInstance)
		{
			// we've got a problem, and the most likely reason is that the location at the end of the instanceSource didn't have
			// any children, and was empty. So don't bother creating any object in the scene.
			// Note: While it might seem like not even adding the items to the instances map would be better, it's quite useful
			//       having a NULL object in the lookup map, as it prevents us from continually doing the very expensive
			//       iterator.getRoot().getByPath() lookup for no reason for all instance locations which point to the empty location.

			return nullInfo;
		}

		return ii;
	}
	else
	{
		// do the expensive lookup of the item...
		FnKat::FnScenegraphIterator itInstanceSource = iterator.getRoot().getByPath(instanceSourcePath);
		if (!itInstanceSource.isValid())
		{
			// TODO: maybe add a placeholder to the map so that we don't try to uselessly attempt to find
			// it using getByPath() (which is expensive) in the future?
			return nullInfo;
		}

		bool isSingleLeaf = !itInstanceSource.getFirstChild().isValid();

		// check two levels down, as that's more conventional...
		FnKat::FnScenegraphIterator itSubItem = itInstanceSource.getFirstChild();
		bool hasSubLeaf = !isSingleLeaf && (itSubItem.isValid() && !itSubItem.getNextSibling().isValid() && !itSubItem.getFirstChild().isValid());
		// if it's simply pointing to a mesh (so a leaf without a hierarchy)
		if (isSingleLeaf || hasSubLeaf)
		{
			// just build the source geometry and link to it....

			if (hasSubLeaf)
			{
				itInstanceSource = itSubItem;
			}

			bool isSubD = m_creationSettings.m_enableSubdivision && itInstanceSource.getType() == "subdmesh";

			FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);

			CompactGeometryInstance* pNewInstance = createCompactGeometryInstanceFromLocation(itInstanceSource, isSubD, imagineStatements);

			unsigned int customFlags = getCustomGeoFlags();
			pNewInstance->setCustomFlags(customFlags);

			Material* pInstanceSourceMaterial = m_materialHelper.getOrCreateMaterialForLocation(itInstanceSource, imagineStatements);

			InstanceInfo ii;
			ii.m_compound = false;
			ii.pGeoInstance = pNewInstance;
			ii.pSingleItemMaterial = pInstanceSourceMaterial;

			m_aInstances[instanceSourcePath] = ii;

			// if we don't have a valid GeometryInstance, don't bother creating a mesh as there's no corresponding geometry.
			// However it's worth adding the NULL item to the instances map so that the expensive lookup of the SG iterator
			// based off the location name isn't continually done, so make sure that's done before we return.

			if (!pNewInstance)
			{
				return nullInfo;
			}

			registerGeometryInstance(pNewInstance);

			return ii;
		}
		else
		{
			// it's a full hierarchy, so we can specialise by building a compound object containing all the sub-objects

			// -1 isn't right, but it works due to the fact that it effectively strips off the base level transform, which
			// is what we want...
			CompoundObject* pCO = createCompoundObjectFromLocation(itInstanceSource, -1);

			if (!pCO)
			{
				// TODO: add NULL item to the map?
				return nullInfo;
			}

			InstanceInfo ii;
			ii.m_compound = true;
			ii.pCompoundObject = pCO;

			m_aInstances[instanceSourcePath] = ii;

			return ii;
		}
	}

	return nullInfo;
}

void SGLocationProcessor::processInstance(FnKat::FnScenegraphIterator iterator)
{
	FnKat::StringAttribute instanceSourceAttribute = iterator.getAttribute("geometry.instanceSource");
	if (!instanceSourceAttribute.isValid())
	{
		fprintf(stderr, "imagineKatana: No geometry.instanceSource attribute specified on instance location: %s\n", iterator.getFullName().c_str());
		return;
	}

	std::string instanceSourcePath = instanceSourceAttribute.getValue("", false);
	if (instanceSourcePath.empty())
	{
		fprintf(stderr, "imagineKatana: Empty geometry.instanceSource attribute specified on instance location: %s\n", iterator.getFullName().c_str());
		return;
	}

	bool isSingleObject = true;

	//

	InstanceInfo instanceInfo = findOrBuildInstanceSourceItem(iterator, instanceSourcePath);
	if (!instanceInfo.pCompoundObject && !instanceInfo.pGeoInstance)
	{
		fprintf(stderr, "imagineKatana: Failed to build instance source: %s\n", instanceSourcePath.c_str());
		return;
	}

	Object* pNewObject = NULL;

	if (instanceInfo.m_compound)
	{
		CompoundInstance* pNewCI = new CompoundInstance(instanceInfo.pCompoundObject);
		pNewObject = pNewCI;

		isSingleObject = false;
	}
	else
	{
		CompactMesh* pNewMesh = new CompactMesh();
		pNewMesh->setCompactGeometryInstance(static_cast<CompactGeometryInstance*>(instanceInfo.pGeoInstance));

		pNewObject = pNewMesh;
		
		// because we're using the common CompactMesh class for single instance items, we need to set this flag for the moment, so that baked geo instances
		// identify instances correctly...
		pNewObject->setFlag(OBJECT_FLAG_INSTANCE);
	}

	// TODO: add per-instance attributes

	if (!pNewObject)
		return;

	//

	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);
	processVisibilityAttributes(imagineStatements, pNewObject);
	
	if (isSingleObject)
	{
		// if it's a single object, we can assign a material to it, so look for one...

		// we don't want to fall back to the default in this case, as we'll use the source instance's material if there is one
		Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator, imagineStatements, false);

		if (pMaterial)
		{
			pNewObject->setMaterial(pMaterial);
		}
		else
		{
			// otherwise set the material to be the source instance's material
			pNewObject->setMaterial(instanceInfo.pSingleItemMaterial);
		}
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

	if (m_pIDState)
	{
		unsigned int objectID = sendObjectID(iterator);
		pNewObject->setObjectID(objectID);
	}

	addObjectToScene(pNewObject, iterator);
}

void SGLocationProcessor::processInstanceArray(FnKat::FnScenegraphIterator iterator)
{
	FnKat::StringAttribute instanceSourceAttribute = iterator.getAttribute("geometry.instanceSource");
	if (!instanceSourceAttribute.isValid())
	{
		fprintf(stderr, "imagineKatana: No geometry.instanceSource attribute specified on instance array location: %s\n", iterator.getFullName().c_str());
		return;
	}

	std::string instanceSourcePath = instanceSourceAttribute.getValue("", false);
	if (instanceSourcePath.empty())
	{
		fprintf(stderr, "imagineKatana: Empty geometry.instanceSource attribute specified on instance array location: %s\n", iterator.getFullName().c_str());
		return;
	}

	InstanceInfo instanceInfo = findOrBuildInstanceSourceItem(iterator, instanceSourcePath);
	if (!instanceInfo.pCompoundObject && !instanceInfo.pGeoInstance)
	{
		fprintf(stderr, "imagineKatana: Failed to build instance source: %s\n", instanceSourcePath.c_str());
		return;
	}

	// instance array matrices can be float or double...
	// look for float version first...
	FnKat::FloatAttribute instanceMatrixAttributeF = iterator.getAttribute("geometry.instanceMatrix");
	FnKat::DoubleAttribute instanceMatrixAttributeD;

	bool isDoubleVersion = false;
	int64_t numValues;

	if (!instanceMatrixAttributeF.isValid())
	{
		instanceMatrixAttributeD = iterator.getAttribute("geometry.instanceMatrix");

		if (!instanceMatrixAttributeD.isValid())
		{
			fprintf(stderr, "imagineKatana: No geometry.instanceMatrix attribute specified on location: %s\n", iterator.getFullName().c_str());
			return;
		}
		else
		{
			numValues = instanceMatrixAttributeD.getNumberOfValues();
			isDoubleVersion = true;
		}
	}
	else
	{
		numValues = instanceMatrixAttributeF.getNumberOfValues();
	}

	// for the moment, we assume no motion blur, so there's only a single time sample, and we're assuming it's a flat
	// list of the 4x4 matrix components, so check we have a multiple of 16
	bool validMatrixLength = (numValues % 16) == 0;
	if (!validMatrixLength)
	{
		fprintf(stderr, "imagineKatana: Error: incorrect number of values specified for 'instanceMatrix' attribute on location: %s\n", iterator.getFullName().c_str());
		return;
	}
	
	// get hold of the location hierarchy xform
	// unfortunately, due to the way we're creating these and we don't have a graphics state hierarchy,
	// we have to manually concat the matrices for each instance item ourself, which isn't great, but...
	FnKat::RenderOutputUtils::XFormMatrixVector xforms = KatanaHelpers::getXFormMatrixStatic(iterator);
	Matrix4 baseTransform;
	baseTransform.setFromArray(xforms[0].getValues(), true);
	
	bool isIdentityBaseTransform = baseTransform.isIdentity();

	FnKat::FloatConstVector matrixValuesF;
	FnKat::DoubleConstVector matrixValuesD;

	size_t fullSize = 0;

	if (!isDoubleVersion)
	{
		matrixValuesF = instanceMatrixAttributeF.getNearestSample(0.0f);
		fullSize = matrixValuesF.size();
	}
	else
	{
		matrixValuesD = instanceMatrixAttributeD.getNearestSample(0.0f);
		fullSize = matrixValuesD.size();
	}

	size_t numInstances = fullSize / 16;
	size_t posIndex = 0;

	bool isCompound = instanceInfo.m_compound;

	Object* pNewObject = NULL;
	
	// In the interests of efficiency, only bother asking for one ID for the location which all resulting
	// instances will share.
	// Note: with heavily-instanced scenes, there's around a 20% overhead at expansion time
	//       for requesting IDs for each instance, and it's much more noticable with instance arrays
	//       as the request to FnKat::Render::IdSenderFactory (which does socket communication and probably
	//       locks internally) is not amortised as much with instance arrays.
	unsigned int objectID = 0;
	if (m_pIDState)
	{
		objectID = sendObjectID(iterator);
	}
	
	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);
	unsigned char renderVisibilityFlags = getRenderVisibilityFlags(imagineStatements);
	
	// we don't want to fall back to the default in this case, as we'll use the source instance's material if there is one
	Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator, imagineStatements, false);

	for (size_t i = 0; i < numInstances; i++)
	{
		// TODO: check the compiler's unrolling this correctly...
		float tempValues[16];
		if (!isDoubleVersion)
		{
			for (unsigned int j = 0; j < 16; j++)
			{
				tempValues[j] = matrixValuesF[posIndex++];
			}
		}
		else
		{
			for (unsigned int j = 0; j < 16; j++)
			{
				tempValues[j] = (float)matrixValuesD[posIndex++];
			}
		}

		if (isCompound)
		{
			CompoundInstance* pNewCI = new CompoundInstance(instanceInfo.pCompoundObject);
			pNewObject = pNewCI;
		}
		else
		{
			CompactMesh* pNewMesh = new CompactMesh();
			pNewMesh->setCompactGeometryInstance(static_cast<CompactGeometryInstance*>(instanceInfo.pGeoInstance));
			pNewObject = pNewMesh;

			if (pMaterial)
			{
				pNewObject->setMaterial(pMaterial);
			}
			else
			{
				// otherwise set the material to be the source instance's material
				pNewObject->setMaterial(instanceInfo.pSingleItemMaterial);
			}
			
			// because we're using the common CompactMesh class for single instance items, we need to set this flag for the moment, so that baked geo instances
			// identify instances correctly...
			pNewObject->setFlag(OBJECT_FLAG_INSTANCE);
		}
		
		// it shaves a tiny bit of expansion time off specialising doing this, so...
		if (isIdentityBaseTransform)
		{
			// if our location has no overall xform, we can just set individual matrix directly, which is slightly faster
			// than doing a concat transform with an identity matrix when processing millions of instances
			
			pNewObject->transform().setCachedMatrix(tempValues, true);
		}
		else
		{
			// we have to do the concatenation of transforms ourselves...
			Matrix4 finalTransform = Matrix4::multiply(baseTransform, Matrix4(tempValues));
			pNewObject->transform().setCachedMatrix(finalTransform);
		}
		
		if (objectID != 0)
		{
			pNewObject->setObjectID(objectID);
		}
		
		pNewObject->setRenderVisibilityFlags(renderVisibilityFlags);

		addObjectToScene(pNewObject, iterator);
	}
}

void SGLocationProcessor::processSphere(FnKat::FnScenegraphIterator iterator)
{
	float radius = 1.0f;

	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (geometryAttribute.isValid())
	{
		FnKat::DoubleAttribute radiusAttr = geometryAttribute.getChildByName("radius");
		if (radiusAttr.isValid())
		{
			radius = radiusAttr.getValue(1.0f, false);
		}
	}

	Sphere* pSphere = new Sphere((float)radius, 16);

	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);
	Material* pMaterial = m_materialHelper.getOrCreateMaterialForLocation(iterator, imagineStatements);
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
	
	processVisibilityAttributes(imagineStatements, pSphere);

	if (m_pIDState)
	{
		unsigned int objectID = sendObjectID(iterator);
		pSphere->setObjectID(objectID);
	}

	addObjectToScene(pSphere, iterator);
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

	FnKat::GroupAttribute imagineStatements = iterator.getAttribute("imagineStatements", true);
	processVisibilityAttributes(imagineStatements, pNewLight);

	addObjectToScene(pNewLight, iterator);
}

unsigned char SGLocationProcessor::getRenderVisibilityFlags(const FnKat::GroupAttribute& imagineStatements)
{
	if (!imagineStatements.isValid())
		return RENDER_VISIBILITY_FLAGS_ALL;
	
	FnKat::GroupAttribute visibilityAttributes = imagineStatements.getChildByName("visibility");
	if (!visibilityAttributes.isValid())
		return RENDER_VISIBILITY_FLAGS_ALL;
	
	unsigned char finalVisibility = 0;
	
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
	
	return finalVisibility;
}

void SGLocationProcessor::processVisibilityAttributes(const FnKat::GroupAttribute& imagineStatements, Object* pObject)
{
	unsigned char renderVisibilityFlags = getRenderVisibilityFlags(imagineStatements);

	pObject->setRenderVisibilityFlags(renderVisibilityFlags);
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

unsigned int SGLocationProcessor::sendObjectID(FnKat::FnScenegraphIterator iterator)
{
	int64_t objectID = m_pIDState->getNextID();

	// only send the ID if it's greater than 0, otherwise it's invalid or we've run out of IDs (Katana only gives us 1000000)...
	// If we send an ID Katana doesn't know about, renderboot gets a SIGPIPE, doesn't handle it correctly
	// and then katanaBin dies during render, so we need to be careful on our side.
	if (objectID > 0)
	{
		std::string name = iterator.getFullName();

		m_pIDState->sendID(objectID, name.c_str());
	}

	return static_cast<unsigned int>(objectID);
}

unsigned int SGLocationProcessor::getCustomGeoFlags()
{
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

	return customFlags;
}
