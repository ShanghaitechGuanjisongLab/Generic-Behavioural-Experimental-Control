#pragma once
#include <stdint.h>
enum class UID : uint8_t {
	// Arduino提供的服务端口

	PortA_PointerSize,
	PortA_CreateProcess,
	PortA_StartModule,
	PortA_RestoreModule,
	PortA_PauseProcess,
	PortA_ContinueProcess,
	PortA_AbortProcess,
	PortA_GetInformation,
	PortA_DeleteProcess,
	PortA_AllProcesses,
	PortA_RandomSeed,
	PortA_IsReady,

	// Computer提供的服务端口

	PortC_ProcessFinished,
	PortC_Signal,
	PortC_TrialStart,
	PortC_Exception,
	PortC_ImReady,
	PortC_Debug,

	// 运行时异常

	Exception_Success,
	Exception_InvalidProcess,
	Exception_MethodNotSupported,
	Exception_StillRunning,
	Exception_ProcessNotRunning,
	Exception_ProcessNotPaused,
	Exception_ProcessNotIdle,
	Exception_ProcessFinished,
	Exception_BrokenStartArguments,
	Exception_BrokenRestoreArguments,
	Exception_MethodNotImplemented,
	Exception_InvalidModule,

	// 信息字段

	Field_After,
	Field_Do,
	Field_ID,
	Field_RandomDuration,
	Field_Duration,
	Field_AbortStep,
	Field_StartModule,
	Field_Modules,
	Field_Range,
	Field_Target,
	Field_Pin,
	Field_Monitor,
	Field_Content,
	Field_Message,
	Field_HighOrLow,
	Field_Cleaner,
	Field_Period,
	Field_Times,
	Field_PeriodA,
	Field_PeriodB,
	Field_ContentA,
	Field_ContentB,
	Field_TargetID,
	Field_Slot,

	// 表列

	Column_SubSteps,
	Column_Repeats,
	Column_Module,

	// 信息字段数据类型

	Type_Bool,
	Type_UInt8,
	Type_UInt16,
	Type_UID,
	Type_Array,
	Type_Struct,
	Type_Seconds,
	Type_Milliseconds,
	Type_Microseconds,
	Type_Infinite,
	Type_Table,
	Type_Pointer,
	Type_Map,

	// 进程状态

	State_Ready,
	State_Running,
	State_Paused,
	State_Aborted,
	State_Finished,
	State_Normal,
	State_Repeat,
	State_Restore,
	State_Idle,

	// 信号事件

	Event_TrialStart,
	Event_MonitorHit,
	Event_MonitorMiss,
	Event_ProcessPaused,
	Event_ProcessContinue,
	Event_ProcessAborted,
	Event_AudioUp,
	Event_AudioDown,
	Event_LightUp,
	Event_LightDown,
	Event_Water,
	Event_AirPuff,
	Event_HitCount,
	Event_HFImage,
	Event_LFImage,
	Event_Optogenetic,
	Event_Image,
	Event_LowUp,
	Event_LowDown,
	Event_HighUp,
	Event_HighDown,

	// 测试

	Test_BlueLed,
	Test_WaterPump,
	Test_CapacitorReset,
	Test_CapacitorMonitor,
	Test_CD1,
	Test_ActiveBuzzer,
	Test_AirPump,
	Test_HostAction,
	Test_SquareWave,
	Test_RandomFlash,
	Test_LowTone,
	Test_HighTone,

	// 回合

	Trial_Invalid,
	Trial_AudioWater,
	Trial_LightWater,

	// 持续时间类型

	Duration_Random,
	Duration_Infinite,

	// 基础模块

	Module_Sequential,
	Module_RandomSequential,
	Module_Delay,
	Module_Abort,
	Module_Restart,
	Module_Randomize,
	Module_MonitorPin,
	Module_SerialMessage,
	Module_DigitalWrite,
	Module_CleanWhenAbort,
	Module_Async,
	Module_RepeatEvery,
	Module_DoubleRepeat,
	Module_Repeat,
	Module_DigitalToggle,
	Module_AssignModuleID,
	Module_ReferModuleID,
	Module_DynamicSlot,
	Module_LoadSlot,
	Module_ClearSlot,

	// 组合模块

	Using_ResponseWindow,

	// 主机动作

	Host_GratingImage,
	Host_StartRecord,

	// 会话

	Session_AudioWater,
	Session_LightWater,

	MagicByte,

};