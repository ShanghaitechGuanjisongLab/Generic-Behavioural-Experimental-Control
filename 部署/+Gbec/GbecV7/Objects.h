#pragma once
#include "UID.h"
#include <unordered_map>
struct Object
{
	virtual UID Pause(Object *) = 0;
	virtual UID Continue(Object *) = 0;
	virtual UID Stop(Object *) = 0;
	virtual UID GetInformation(Object *, uint8_t ToPort) = 0;
	virtual ~Object();
};
extern const std::unordered_map<UID, Object *(*)()> ObjectCreators;