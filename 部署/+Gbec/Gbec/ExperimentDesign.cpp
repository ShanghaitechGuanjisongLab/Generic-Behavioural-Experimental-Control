#pragma once
#include "Predefined.hpp"
// 快速切换BOX设定集
#define BOX 1

// 引脚设定集
#if BOX == 1
Pin BlueLed = 2;
Pin WaterPump = 3;
Pin CapacitorVdd = 4;
Pin CapacitorOut = 5;
Pin CD1 = 6;
Pin ActiveBuzzer = 7;
Pin AirPump = 8;
Pin Laser = 9;
Pin PassiveBuzzer = 10;
#endif

// 可以将基础模块组合成自定义的复杂模块，可以是有参数的模块模板或无参数的固定模块

template<uint8_t PinIndex, uint16_t Milliseconds>
using PinFlash = Sequential<DigitalWrite<PinIndex, HIGH>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>>;

using Duration100To1000 = RandomDuration<std::chrono::milliseconds, 100, 1000>;

using RandomFlash = Repeat<Sequential<DigitalToggle<Laser>, Delay<Duration100To1000>, ModuleRandomize<Duration100To1000>>>;

using Duration5To10 = RandomDuration<std::chrono::seconds, 5, 10>;

using Delay5To10 = Delay<Duration5To10>;

using MonitorRestart = MonitorPin<CapacitorOut, ModuleRestart<Delay5To10>>;

template<uint16_t Milliseconds>
using DelayMilliseconds = Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>;

template<uint8_t Seconds>
using DelaySeconds = Delay<ConstantDuration<std::chrono::seconds, Seconds>>;

// 注意RepeatEvery和UntilDuration的时间单位必须统一
template<uint16_t FrequencyHz, uint16_t Microseconds>
using Tone = typename RepeatEvery<ConstantDuration<std::chrono::microseconds, 1000000 / FrequencyHz>, DigitalToggle<PassiveBuzzer>>::template UntilDuration<ConstantDuration<std::chrono::microseconds, Microseconds>>;

using CalmDown = Sequential<MonitorRestart, Delay5To10, ModuleAbort<MonitorRestart>, ModuleRandomize<Delay5To10>>;

using AudioWater = Trial<UID::Trial_AudioWater, Sequential<CalmDown, PinFlash<ActiveBuzzer, 200>, DelayMilliseconds<800>, PinFlash<WaterPump, 250>, DelaySeconds<20>>>;

// 列出所有公开模块，允许PC端调用
std::unordered_map<UID, bool (*)(Process *, uint16_t)> SessionMap = {
  { UID::Test_BlueLed, Session<PinFlash<BlueLed, 200>> },
  { UID::Test_WaterPump, Session<PinFlash<WaterPump, 150>> },
  { UID::Test_CapacitorReset, Session<Sequential<DigitalWrite<CapacitorVdd, LOW>, Delay<ConstantDuration<std::chrono::milliseconds, 100>>, DigitalWrite<CapacitorVdd, HIGH>>> },
  { UID::Test_CapacitorMonitor, Session<Sequential<DigitalWrite<CapacitorVdd, HIGH>, MonitorPin<CapacitorOut, SerialMessage<UID::Signal_CapacitorSignal>>>> },
  { UID::Test_CD1, Session<PinFlash<CD1, 200>> },
  { UID::Test_ActiveBuzzer, Session<PinFlash<ActiveBuzzer, 200>> },
  { UID::Test_AirPump, Session<PinFlash<AirPump, 200>> },
  { UID::Test_HostAction, Session<SerialMessage<UID::Host_ShowImage>> },
  { UID::Test_SquareWave, Session<DoubleRepeat<ConstantDuration<std::chrono::seconds, 1>, DigitalWrite<Laser, HIGH>, ConstantDuration<std::chrono::seconds, 2>, DigitalWrite<Laser, LOW>>::template UntilTimes<3>> },
  { UID::Test_RandomFlash, Session<Sequential<Async<RandomFlash>, Delay<ConstantDuration<std::chrono::seconds, 10>>, ModuleAbort<RandomFlash>>> },
  { UID::Test_LowTone, Session<Tone<500, 1000>> },
  { UID::Test_HighTone, Session<Tone<5000, 1000>> },
  { UID::Session_AudioWater, Session<Repeat<AudioWater>::UntilTimes<30>> },
};