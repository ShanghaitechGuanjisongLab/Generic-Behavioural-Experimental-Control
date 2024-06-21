#pragma once
#include <Cpp_Standard_Library.h>
#include <unordered_map>
#include <set>
#include "UID.h"
extern std::set<Routine *> Processes;
struct Routine
{
	// 所有Routine的基类
};
template <UID UID>
struct PinFlashRoutine
{
	static constexpr UID UID = UID;
};
template <typename RoutineType>
Routine *Run()
{
	return new RoutineType();
}
template <typename... RoutineType>
struct RoutineSet
{
	static std::unordered_map<UID, Routine *(*)()> RoutineMap = {{RoutineType::UID, Run<RoutineType>}...};
};