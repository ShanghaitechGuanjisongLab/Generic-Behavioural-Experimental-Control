#pragma once
#include <stdint.h>
enum class UID : uint8_t {
	//Arduino提供的服务端口

	PortA_PointerSize,
	PortA_CreateProcess,
	PortA_StartProcess,
	PortA_RestoreProcess,
	PortA_RepeatProcess,
	PortA_PauseProcess,
	PortA_ContinueProcess,
	PortA_AbortProcess,
	PortA_GetInformation,
	PortA_DeleteProcess,
	PortA_AllProcesses,
	PortA_IsReady,
	PortA_RandomSeed,

	//Computer提供的服务端口

	PortC_ProcessFinished,
	PortC_Signal,
	PortC_TrialStart,
	PortC_Exception,

	Exception_Success,
	Exception_InvalidProcess,
	Exception_MethodNotSupported,
	Exception_StillRunning,
	Exception_ProcessNotRunning,
	Exception_ProcessNotPaused,
	Exception_ProcessNotIdle,
	Exception_ProcessFinished,
	Exception_BrokenRestoreArguments,
	Exception_MethodNotImplemented,
	
	Field_After,
	Field_Do,

	Column_SubSteps,
	Column_Repeats,

	Type_Bool,
	Type_UInt8,
	Type_UInt16,
	Type_UID,
	Type_Array,
	Type_Cell,
	Type_Struct,
	Type_Empty,
	Type_Seconds,
	Type_Milliseconds,
	Type_Infinite,
	Type_Table,

	State_Ready,
	State_Running,
	State_Paused,
	State_Aborted,
	State_Finished,
	State_Normal,
	State_Repeat,
	State_Restore,
	State_Idle,

	Signal_TrialStart,
	Signal_ProcessFinished,
	Signal_ProcessStarted,

	Test_BlueLed,

	Step_DigitalWrite,
	Step_Delay,
	Step_StartMonitor,
	Step_StopMonitor,
	Step_Trial,
	Step_RepeatIfPin,
	Step_SwitchIfPin,
	Step_AppendIfPin,
	Step_SerialWrite,
	Step_Async,
	Step_Sequential,
	Step_Random,
	Step_CustomFunction,
	Step_DoWhenAborted,
	Step_StartBackgroundRepeat,
	Step_Sequential,
	Step_Randomize,
	Step_RandomSequential,

	Trial_Invalid,

	BackgroundID_Default,

	Public_BlueLed,
};