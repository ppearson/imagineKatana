#ifndef SG_LOCATION_PROCESSOR_H
#define SG_LOCATION_PROCESSOR_H

#include <FnAttribute/FnAttribute.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>


class SGLocationProcessor
{
public:
	SGLocationProcessor();

	static void processSG(FnKat::FnScenegraphIterator rootIterator);
	static void processSGForceExpand(FnKat::FnScenegraphIterator rootIterator);


};

#endif // SG_LOCATION_PROCESSOR_H
