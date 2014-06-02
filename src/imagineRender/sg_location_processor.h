#ifndef SG_LOCATION_PROCESSOR_H
#define SG_LOCATION_PROCESSOR_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>


class SGLocationProcessor
{
public:
	SGLocationProcessor();

	void processSG(FnKat::FnScenegraphIterator rootIterator);
	void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);

	void processLocationRecursive(FnKat::FnScenegraphIterator iterator);

	void processGeometryPolymesh(FnKat::FnScenegraphIterator iterator);

protected:
	bool				m_applyMaterials;
	bool				m_useTextures;
	bool				m_enableSubd;

};

#endif // SG_LOCATION_PROCESSOR_H
