#ifndef SG_LOCATION_PROCESSOR_H
#define SG_LOCATION_PROCESSOR_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#ifndef STAND_ALONE

#include "scene.h"

#endif


class SGLocationProcessor
{
public:
#ifndef STAND_ALONE
	SGLocationProcessor(Scene& scene);
#else
	SGLocationProcessor();
#endif

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator);

	void processGeometryPolymesh(FnKat::FnScenegraphIterator iterator);

protected:
	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubd;

	Scene				m_scene;
};

#endif // SG_LOCATION_PROCESSOR_H
