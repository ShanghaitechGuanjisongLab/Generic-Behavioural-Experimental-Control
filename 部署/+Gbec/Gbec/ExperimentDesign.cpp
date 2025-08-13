#pragma once
#include "Predefined.hpp"
// 快速切换BOX设定集
#define BOX 1
// 引脚设定
#if BOX == 1
Pin pBlueLed = 2;
#endif
constexpr bool (*PM)(Process *, uint16_t) = Session< Sequential<DigitalWrite<pBlueLed, HIGH>,
                                                                Delay<ConstantDuration<std::chrono::milliseconds, 150>>, DigitalWrite< pBlueLed, LOW >>>;
std::unordered_map<UID, bool (*)(Process *, uint16_t)> SessionMap = {
	{ UID::Test_BlueLed, PM }
};