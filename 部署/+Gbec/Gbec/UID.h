#pragma once
#include <stdint.h>
/*
注册各种元素对象的唯一标识符，无论是数据类型、属性还是设备。注册了UID的元素才能在开发板与PC端之间进行交流。在PC端也必须保留一份一模一样的注册表。实际上类似于密码表。
编辑此列表时应当注意类别分区。除了零元和纯枚举类以外，其它类区应当以0起始。同类UID应当连续放置以节约内存。
已定义值的枚举必须全部罗列在列表开头，否则无法自动生成MATLAB枚举类。
*/
enum UID : uint8_t {
  // 监视旗帜，可用按位或|进行组合

  // 只汇报一次
  Monitor_ReportOnce = 0b01,

  // 命中后直接结束步骤
  Monitor_HitAndFinish = 0b10,

  // API，关联数组顺序，不可改变

  API_Start = 0,
  API_Pause,
  API_Continue,
  API_Abort,
  API_Peek,
  API_GetInfo,
  API_Restore,
  API_TestStart,
  API_TestStop,
  API_IsReady,

  //特殊事件

  Event_SerialInterrupt,
  Event_SerialReconnect,

  // 信息类型

  Type_UID,
  Type_Bool,
  Type_UInt8,
  Type_UInt16,
  Type_Array,
  Type_Cell,
  Type_Struct,

  // 信息字段

  Info_UID,
  Info_Random,
  Info_NumTrials,
  Info_DistinctTrials,
  Info_Steps,
  Info_MinMilliseconds,
  Info_MaxMilliseconds,
  Info_Milliseconds,
  Info_ReportOnce,
  Info_ReportMiss,
  Info_HitAndFinish,
  Info_HitReporter,
  Info_MissReporter,
  Info_Reporter,
  Info_Pin,
  Info_ToWrite,
  Info_FrequencyHz,
  Info_HighMilliseconds,
  Info_LowMilliseconds,
  Info_NumCycles,
  Info_RandomCycleMin,
  Info_RandomCycleMax,
  Info_Event,
  Info_MinHighMilliseconds,
  Info_MaxHighMilliseconds,
  Info_MinLowMilliseconds,
  Info_MaxLowMilliseconds,
  Info_SubSteps,

  //状态

  State_SessionInvalid,
  State_SessionRunning,
  State_SessionPaused,
  State_SessionAborted,
  State_SessionFinished,
  //此状态仅限MATLAB端使用，指刚恢复连接，但处于暂停状态
  State_SessionRestored,

  // 信号UID

  Signal_SerialReady,
  Signal_NoSuchSession,
  Signal_SessionStarted,
  Signal_SessionPaused,
  Signal_SessionContinue,
  Signal_SessionRestored,
  Signal_ProgressPlease,
  Signal_TrialStart,
  Signal_NoSuchTest,
  Signal_TestStartedAutoStop,
  Signal_TestStartedManualStop,
  Signal_TestStopped,
  Signal_WaterOffered,
  Signal_LightUp,
  Signal_LightDown,
  Signal_AudioUp,
  Signal_AudioDown,
  Signal_HighUp,
  Signal_HighDown,
  Signal_LowUp,
  Signal_LowDown,
  Signal_MonitorHit,
  Signal_MonitorMiss,
  Signal_NoLastTest,
  Signal_HitCount,
  Signal_StartRecord,
  Signal_AirPuff,
  Signal_InfoStart,
  Signal_ApiFound,
  Signal_ApiInvalid,
  Signal_HostAction,
  Signal_LFImage,
  Signal_HFImage,
  Signal_Laser,
  Signal_Image,
  Signal_PreciseLog,
  Signal_Optogenetic,
  Signal_TimerAllocated,
  Signal_TimerReleased,

  // 测试UID

  //指代上一个运行的测试
  Test_Last,
  Test_CD1,
  Test_BlueLed,
  Test_ActiveBuzzer,
  Test_Water,
  Test_Air,
  Test_CapacitorReset,
  Test_CapacitorMonitor,
  Test_SquareWave,
  Test_RandomFlash,
  Test_HighTone,
  Test_LowTone,
  Test_Optogenetic,

  // 步骤UID

  Step_Calmdown,
  Step_PinFlash,
  Step_StartMonitor,
  Step_StopMonitor,
  Step_Water,
  Step_Light,
  Step_ShortDelay,
  Step_ResponseWindow,
  Step_ITI,
  Step_Null,
  Step_Monitor,
  Step_Wait,
  Step_SerialWrite,
  Step_Audio,
  Step_Tag,
  Step_LowTone,
  Step_HighTone,
  Step_Air,
  Step_SquareWave,
  Step_RandomFlash,
  Step_PreciseLog,
  Step_PinLow,
  Step_Optogenetic,
  Step_StartRandomFlash,
  Step_StopRandomFlash,
  Step_IndividualThread,

  // 回合UID

  Trial_LightOnly,
  Trial_AudioOnly,
  Trial_WaterOnly,
  Trial_LightWater,
  Trial_LightLickWater,
  Trial_AudioWater,
  Trial_LightAir,
  Trial_StartMonitor,
  Trial_StopMonitor,
  Trial_LightDelayWater,
  Trial_HostOnly,
  Trial_LFImage,
  Trial_HFImage,
  Trial_RandomImage,
  Trial_RandomFlash,
  Trial_LowTone,
  Trial_HighTone,
  Trial_OptogeneticLightWaterTest,
  Trial_OptogeneticLightWaterTrain,
  Trial_StartInterfereMonitor,
  Trial_StopInterfereMonitor,
  Trial_OptogeneticBeforeLightWater,
  Trial_OptogeneticUponLightWater,
  Trial_OptogeneticAfterLightWater,
  Trial_OptogeneticBeforeAudioWater,
  Trial_OptogeneticUponAudioWater,
  Trial_OptogeneticAfterAudioWater,

  // 会话UID

  Session_Current,//内部使用，请勿修改
  Session_LAWLwAw,
  Session_LAuW,
  Session_LightWater,
  Session_AudioWater,
  Session_LightAir,
  Session_SurveillanceThroughout,
  Session_HLFImage,
  Session_RandomImage,
  Session_RandomFlash,
  Session_HighLowTone,
  Session_OptogeneticLightWaterTest,
  Session_OptogeneticLightWaterTrain,
  Session_InterferedAudioWater,
  Session_OptogeneticAllTest,
};