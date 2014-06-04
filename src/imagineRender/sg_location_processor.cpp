#include "sg_location_processor.h"

#include <stdio.h>

#include <RenderOutputUtils/RenderOutputUtils.h>

#ifndef STAND_ALONE
#include "objects/mesh.h"
#include "objects/primitives/sphere.h"

#include "geometry/standard_geometry_operations.h"
#include "geometry/standard_geometry_instance.h"

#include "materials/standard_material.h"
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

	// TODO: copy any UVs



	// work out the faces...
	FnKat::GroupAttribute polyAttribute = geometryAttribute.getChildByName("poly");
	FnKat::IntAttribute polyStartIndexAttribute = polyAttribute.getChildByName("startIndex");
	FnKat::IntAttribute vertexListAttribute = polyAttribute.getChildByName("vertexList");

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	std::deque<Face>& aFaces = pNewGeoInstance->getFaces();
	unsigned int currentVertexIndex = 0;

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
			unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex++];

			newFace.addVertex(vertexIndex);
		}

		// for the moment do this, but we should read the normals directly from the geometry attribute in Katana really
		// as we can't get per-mesh crease threshold info from Katana and there are occasionally back-facing issues...
		newFace.calculateNormal(pNewGeoInstance);

		aFaces.push_back(newFace);
	}

	// again, we should do things properly here, but this is a good start to get things going...
	StandardGeometryOperations::calculateVertexNormals(pNewGeoInstance);
	StandardGeometryOperations::tesselateGeometryToTriangles(pNewGeoInstance, true);

	// and again...
	pNewGeoInstance->calculateBoundaryBox();

	Mesh* pNewMeshObject = new Mesh();

	pNewMeshObject->setGeometryInstance(pNewGeoInstance);

	// for the moment, assign default
	StandardMaterial* pNewMat = new StandardMaterial();
	pNewMeshObject->setMaterial(pNewMat);

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

	pNewMeshObject->transform().setCachedMatrix(pMatrix);

	m_scene.addObject(pNewMeshObject, false, false);
}
