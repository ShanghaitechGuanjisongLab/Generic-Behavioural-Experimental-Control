#pragma once
#include <stdint.h>
enum class UID : uint8_t
{
	Port_PointerSize,
	Port_ObjectCreate,
	Port_ObjectStart,
	Port_ObjectRestore,
	Port_ObjectRepeat,
	Port_ObjectPause,
	Port_ObjectContinue,
	Port_ObjectAbort,
	Port_ObjectGetInformation,
	Port_ObjectDestroy,
	Port_AllObjects,
	Port_RandomSeed,

	Exception_Success,
	Exception_InvalidObject,
	Exception_MethodNotSupported,
	Exception_StillRunning,
	Exception_ObjectNotIdle,
	Exception_ObjectNotRunning,
	Exception_ObjectNotPaused,

	Property_TemplateID,
	Property_Subobjects,

	TemplateID_Sequential,
	TemplateID_SequentialRepeat,

	Type_Bool,
	Type_UInt8,
	Type_UInt16,
	Type_UID,
	Type_Array,
	Type_Cell,
	Type_Struct,

	State_ObjectRunning,
	State_ObjectPaused,
	State_ObjectIdle,

	Progress_Exception,
	Progress_Custom,
	Progress_Trial,
};