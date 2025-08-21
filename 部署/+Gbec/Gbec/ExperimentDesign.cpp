#pragma once
#include "Predefined.hpp"
// 快速切换BOX设定集
#define BOX 1

// 引脚设定集
#if BOX == 1
Pin BlueLed = 8;
Pin WaterPump = 2;
Pin CapacitorVdd = 7;
Pin CapacitorOut = 18;
Pin CD1 = 6;
Pin ActiveBuzzer = 22;
Pin AirPump = 12;
Pin Laser = 53;
Pin PassiveBuzzer = 3;
#endif

// 可以将基础模块组合成自定义的复杂模块，可以是有参数的模块模板或无参数的固定模块

template<uint8_t PinIndex, uint16_t Milliseconds>
using PinFlash = Sequential<DigitalWrite<PinIndex, HIGH>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>>;

template<uint8_t PinIndex, uint16_t Milliseconds, UID Up>
using PinFlashUp = Sequential<DigitalWrite<PinIndex, HIGH>, SerialMessage<Up>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>>;

template<uint8_t PinIndex, uint16_t Milliseconds, UID Up, UID Down>
using PinFlashUpDown = Sequential<DigitalWrite<PinIndex, HIGH>, SerialMessage<Up>, Delay<ConstantDuration<std::chrono::milliseconds, Milliseconds>>, DigitalWrite<PinIndex, LOW>, SerialMessage<Down>>;

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
template<uint16_t FrequencyHz, uint16_t Milliseconds>
using Tone = typename RepeatEvery<ConstantDuration<std::chrono::microseconds, 1000000 / FrequencyHz>, DigitalToggle<PassiveBuzzer>>::template UntilDuration<ConstantDuration<std::chrono::microseconds, Milliseconds * 1000>>;

using CalmDown = Sequential<MonitorRestart, Delay5To10, ModuleAbort<MonitorRestart>, ModuleRandomize<Delay5To10>>;

using AudioWater = Trial<UID::Trial_AudioWater, Sequential<CalmDown, PinFlashUpDown<ActiveBuzzer, 200, UID::Event_AudioUp, UID::Event_AudioDown>, DelayMilliseconds<800>, PinFlashUp<WaterPump, 150, UID::Event_Water>, DelaySeconds<20>>>;

// 列出所有公开模块，允许PC端调用
std::unordered_map<UID, bool (*)(Process*, uint16_t, uint16_t&)> SessionMap = {
  { UID::Test_BlueLed, Session<PinFlash<BlueLed, 200>> },
  { UID::Test_WaterPump, Session<PinFlash<WaterPump, 150>> },
  { UID::Test_CapacitorReset, Session<Sequential<DigitalWrite<CapacitorVdd, LOW>, Delay<ConstantDuration<std::chrono::milliseconds, 100>>, DigitalWrite<CapacitorVdd, HIGH>>> },
  { UID::Test_CapacitorMonitor, Session<Sequential<DigitalWrite<CapacitorVdd, HIGH>, MonitorPin<CapacitorOut, SerialMessage<UID::Event_MonitorHit>>>> },
  { UID::Test_CD1, Session<PinFlash<CD1, 200>> },
  { UID::Test_ActiveBuzzer, Session<PinFlash<ActiveBuzzer, 200>> },
  { UID::Test_AirPump, Session<PinFlash<AirPump, 200>> },
  { UID::Test_HostAction, Session<SerialMessage<UID::Host_GratingImage>> },
  { UID::Test_SquareWave, Session<DoubleRepeat<ConstantDuration<std::chrono::seconds, 1>, DigitalWrite<Laser, HIGH>, ConstantDuration<std::chrono::seconds, 2>, DigitalWrite<Laser, LOW>>::template UntilTimes<3>> },
  { UID::Test_RandomFlash, Session<Sequential<Async<RandomFlash>, Delay<ConstantDuration<std::chrono::seconds, 10>>, ModuleAbort<RandomFlash>>> },
  { UID::Test_LowTone, Session<Tone<500, 1000>> },
  { UID::Test_HighTone, Session<Tone<5000, 1000>> },
  { UID::Session_AudioWater, Session<Repeat<AudioWater>::UntilTimes<30>> },
};