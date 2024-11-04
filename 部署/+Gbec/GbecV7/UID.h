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
	Exception_ProgressObjectNotFound,
	Exception_AsyncObjectNotFound,

	Property_ClassID,
	Property_Subobjects,
	Property_ObjectInfo,
	Property_RepeatTime,
	Property_SignalValue,
	Property_StarterID,

	TemplateID_Sequential,
	TemplateID_SequentialRepeat,
	TemplateID_Random,
	TemplateID_RandomRepeat,
	TemplateID_Signal,
	TemplateID_TrialStart,
	TemplateID_StartAsync,
	TemplateID_StopAsync,

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
	Progress_Overwrite,
	Progress_Signal,
	Progress_Append,
	Progress_Warning,

	Signal_TrialStart,
};