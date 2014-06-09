#include "sg_location_processor.h"

#include <stdio.h>

#include <RenderOutputUtils/RenderOutputUtils.h>

#ifndef STAND_ALONE
#include "objects/mesh.h"
#include "objects/primitives/sphere.h"

#include "geometry/standard_geometry_operations.h"
#include "geometry/standard_geometry_instance.h"

#include "lights/light.h"
#endif

#ifndef STAND_ALONE
SGLocationProcessor::SGLocationProcessor(Scene& scene) : m_scene(scene), m_applyMaterials(true), m_useTextures(true), m_enableSubd(true)
{
}
#else
SGLocationProcessor::SGLocationProcessor() : m_applyMaterials(true), m_useTextures(true), m_enableSubd(true)
{
}
#endif

void SGLocationProcessor::processSG(FnKat::FnScenegraphIterator rootIterator)
{

}

void SGLocationProcessor::processSGForceExpand(FnKat::FnScenegraphIterator rootIterator)
{
	processLocationRecursive(rootIterator);
}

void SGLocationProcessor::processLocationRecursive(FnKat::FnScenegraphIterator iterator)
{
	std::string type = iterator.getType();

	FnKat::FnScenegraphIterator child = iterator.getFirstChild();
	for (; child.isValid(); child = child.getNextSibling())
	{
		processLocationRecursive(child);
	}

	if (type == "polymesh" || type == "subdmesh")
	{
		// treat subdmesh as polymesh for the moment...

		// TODO: use SG location delegates...

		processGeometryPolymesh(iterator);
	}
	else if (type == "sphere" || type == "nurbspatch") // hack, but works for now...
	{
		processSphere(iterator);
	}
	else if (type == "light")
	{
		processLight(iterator);
	}
}

void SGLocationProcessor::processGeometryPolymesh(FnKat::FnScenegraphIterator iterator)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: polymesh: %s does not have a geometry attribute...", name.c_str());
		return;
	}

	StandardGeometryInstance* pNewGeoInstance = new StandardGeometryInstance();

	std::deque<Point>& aPoints = pNewGeoInstance->getPoints();

	// copy across the points...

	FnKat::GroupAttribute pointAttribute = geometryAttribute.getChildByName("point");

	// linear list of components of Vec3 points
	FnKat::FloatAttribute pAttr = pointAttribute.getChildByName("P");

	FnKat::FloatConstVector sampleData = pAttr.getNearestSample(0.0f);

	unsigned int numItems = sampleData.size();

	// convert to Point items
	for (unsigned int i = 0; i < numItems; i += 3)
	{
		float x = sampleData[i];
		float y = sampleData[i + 1];
		float z = sampleData[i + 2];

		aPoints.push_back(Point(x, y, z));
	}

	FnKat::IntAttribute uvIndexAttribute;
	bool hasUVs = false;
	bool indexedUVs = false;
	// copy any UVs
	FnKat::GroupAttribute stAttribute = iterator.getAttribute("geometry.arbitrary.st", true);
	if (stAttribute.isValid())
	{
		FnKat::RenderOutputUtils::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);

		if (arbitraryAttribute.isValid())
		{
			FnKat::FloatAttribute uvItemAttribute;
			if (arbitraryAttribute.hasIndexedValueAttr())
			{
				uvIndexAttribute = arbitraryAttribute.getIndexAttr(true);
				uvItemAttribute = arbitraryAttribute.getIndexedValueAttr();
				indexedUVs = true;
			}
			else
			{
				// otherwise, just build the list of vertices sequentially later
				uvItemAttribute = arbitraryAttribute.getValueAttr();
			}

			if (uvItemAttribute.isValid())
			{
				hasUVs = true;
				std::deque<UV>& aUVs = pNewGeoInstance->getUVs();

				FnKat::FloatConstVector uvlist = uvItemAttribute.getNearestSample(0);
				unsigned int numItems = uvlist.size();

				for (unsigned int i = 0; i < numItems; i += 2)
				{
					const float& u = uvlist[i];
					const float& v = uvlist[i + 1];

					aUVs.push_back(UV(u, v));
				}
			}
		}
	}


	// work out the faces...
	FnKat::GroupAttribute polyAttribute = geometryAttribute.getChildByName("poly");
	FnKat::IntAttribute polyStartIndexAttribute = polyAttribute.getChildByName("startIndex");
	FnKat::IntAttribute vertexListAttribute = polyAttribute.getChildByName("vertexList");

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	std::deque<Face>& aFaces = pNewGeoInstance->getFaces();
	unsigned int currentVertexIndex = 0;

	if (hasUVs && indexedUVs)
	{
		// we could just generate the UV indices in the un-indexed case and use the same code-path for both
		// like the Arnold plugin does, but that's extra memory allocation which profiling seems to show
		// is slower, probably because we're doing allocation for the UVs on the Imagine side in Face::addUV()
		// anyway...
		// Once this is converted to use CompactGeometryInstance, this won't be an issue any more...

		FnKat::IntConstVector uvIndices = uvIndexAttribute.getNearestSample(0.0f);

		// the last item is extra, and doesn't designate an actual poly, so we can simply do...
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

			Face newFace(numVertices);

			// now use this number to work out how many vertices we need for this face
			for (unsigned j = 0; j < numVertices; j++)
			{
				unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex];

				newFace.addVertex(vertexIndex);

				unsigned int uvIndex = uvIndices[currentVertexIndex++];

				newFace.addUV(uvIndex);
			}

			// for the moment do this, but we should read the normals directly from the geometry attribute in Katana really
			// as we can't get per-mesh crease threshold info from Katana and there are occasionally back-facing issues...
			newFace.calculateNormal(pNewGeoInstance);

			aFaces.push_back(newFace);
		}
	}
	else
	{
		// the last item is extra, and doesn't designate an actual poly, so we can simply do...
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

			Face newFace(numVertices);

			if (hasUVs)
			{
				// we're unindexed, so we should have one UV per vertex...

				// now use this number to work out how many vertices we need for this face
				for (unsigned j = 0; j < numVertices; j++)
				{
					unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex++];

					newFace.addVertex(vertexIndex);
					newFace.addUV(vertexIndex);
				}
			}
			else
			{
				// now use this number to work out how many vertices we need for this face
				for (unsigned j = 0; j < numVertices; j++)
				{
					unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex++];

					newFace.addVertex(vertexIndex);
					newFace.addUV(vertexIndex);
				}
			}

			// for the moment do this, but we should read the normals directly from the geometry attribute in Katana really
			// as we can't get per-mesh crease threshold info from Katana and there are occasionally back-facing issues...
			newFace.calculateNormal(pNewGeoInstance);

			aFaces.push_back(newFace);
		}
	}

	// again, we should do things properly here, but this is a good start to get things going...
	StandardGeometryOperations::calculateVertexNormals(pNewGeoInstance);

	if (hasUVs)
	{
		pNewGeoInstance->setHasPerVertexUVs(true);
	}

	StandardGeometryOperations::tesselateGeometryToTriangles(pNewGeoInstance, true);

	FnKat::DoubleAttribute boundAttr = iterator.getAttribute("bound");
	if (boundAttr.isValid())
	{
		FnKat::DoubleConstVector doubleValues = boundAttr.getNearestSample(0.0f);
		BoundaryBox bbox;
		bbox.getMinimum() = Vector(doubleValues.at(0), doubleValues.at(2), doubleValues.at(4));
		bbox.getMaximum() = Vector(doubleValues.at(1), doubleValues.at(3), doubleValues.at(5));
		pNewGeoInstance->setBoundaryBox(bbox);
	}
	else
	{
		// strictly-speaking, this means that the attributes are wrong, so we should probably ignore it, but
		// on the basis that we're force-expanding everything currently anyway...
		pNewGeoInstance->calculateBoundaryBox();
	}

	Mesh* pNewMeshObject = new Mesh();

	pNewMeshObject->setGeometryInstance(pNewGeoInstance);

	FnKat::GroupAttribute materialAttrib = m_materialHelper.getMaterialForLocation(iterator);
	std::string materialHash = m_materialHelper.getMaterialHash(materialAttrib);

	// see if we've got it already
	Material* pMaterial = m_materialHelper.getExistingMaterial(materialHash);

	if (!pMaterial)
	{
		// create it
		pMaterial = m_materialHelper.createNewMaterial(materialHash, materialAttrib);
	}

	pNewMeshObject->setMaterial(pMaterial);

	// do transform

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pNewMeshObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pNewMeshObject, false, false);
}

void SGLocationProcessor::processSphere(FnKat::FnScenegraphIterator iterator)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: sphere: %s does not have a geometry attribute...", name.c_str());
		return;
	}

	FnKat::DoubleAttribute radiusAttr = geometryAttribute.getChildByName("radius");

	if (!radiusAttr.isValid())
		return;

	double radius = radiusAttr.getValue(1.0f, false);

	Sphere* pSphere = new Sphere((float)radius, 24);

	FnKat::GroupAttribute materialAttrib = m_materialHelper.getMaterialForLocation(iterator);
	std::string materialHash = m_materialHelper.getMaterialHash(materialAttrib);

	// see if we've got it already
	Material* pMaterial = m_materialHelper.getExistingMaterial(materialHash);

	if (!pMaterial)
	{
		// create it
		pMaterial = m_materialHelper.createNewMaterial(materialHash, materialAttrib);
	}

	pSphere->setMaterial(pMaterial);

	// do transform

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pSphere->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pSphere, false, false);
}

void SGLocationProcessor::processLight(FnKat::FnScenegraphIterator iterator)
{
	FnKat::GroupAttribute lightMaterialAttrib = m_materialHelper.getMaterialForLocation(iterator);

	Light* pNewLight = m_lightHelper.createLight(lightMaterialAttrib);

	if (!pNewLight)
		return;

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pNewLight->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pNewLight, false, false);
}
