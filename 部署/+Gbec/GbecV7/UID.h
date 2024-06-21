#pragma once
#include <stdint.h>
enum class UID : uint8_t
{
	Command_StartProcess,
	Command_PauseProcess,
	Command_ContinueProcess,
	Command_KillProcess,
	Command_RunningProcesses,
	Command_ModuleInfo,

	Signal_SerialReady,
	Signal_CommandSuccess,
};