#pragma once
#include <stdint.h>
enum class UID : uint8_t
{
	Port_PointerSize,
	Port_ObjectCreate,
	Port_ObjectPause,
	Port_ObjectContinue,
	Port_ObjectStop,
	Port_ObjectGetInformation,
	Port_ObjectDestroy,
	Port_AllObjects,
};