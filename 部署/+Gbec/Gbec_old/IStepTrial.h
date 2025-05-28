#pragma once
#include "UID.h"
// 具体Step的设计应当具有概念性，优先考虑其输出信息易于理解
struct IStep {
  //会话开始前将被所在回合调用一次，通常用于设置pinMode等初始化状态。如果不需要初始化可以不继承此方法。
  virtual void Setup() const {}
  /*
  步骤开始时，此方法被所在回合调用。步骤可分为立即完成、无需等待直接进入下一步骤的即时步骤，和需要等待一段时间才能完成的延时步骤。
  对于即时步骤，可以直接忽略FinishCallback参数，执行完步骤内容后返回false即可，控制权回到回合后将立即进入下一步骤。
  对于延时步骤，应当保存FinishCallback，然后返回true，提示回合必须等待该步骤调用FinishCallback。步骤应当设置一个时间和/或条件中断，在中断处理函数中调用FinishCallback表示步骤结束，控制权回到回合。
  参考Predefined.h中预先定义的步骤：
  PinFlashStep就是一个典型的即时步骤，它将引脚设置为高电平，然后用一个计时器中断在一定时间后将其设为低电平；但是步骤本身不占用主轴时间，立即返回true，进入下一步。
  WaitStep则是一个简单的延时步骤，它设置一个计时器中断，在一定时间后调用FinishCallback，本身则返回false，提示回合应当等待。而不进入下一步。
  */
  virtual bool Start(void (*FinishCallback)()) const {};
  // 此方法并非必需实现。对于即时步骤，因为步骤立即完成不会被暂停，不需要实现此方法。对于延时步骤，一般应当实现此方法，否则暂停指令将被忽略。
  virtual void Pause() const {}
  //如果你实现了Pause，那么显然也必须有一个对应的Continue实现。
  virtual void Continue() const {}
  // 默认和暂停相同，如果没有特殊的Abort实现就不需要重写
  virtual void Abort() const {
    Pause();
  }
  static constexpr UID Info = Step_Null;
};
struct ITrial : public IStep {
  UID MyUID;

protected:
  static uint8_t StepsDone;
  constexpr ITrial(UID MyUID)
    : MyUID(MyUID) {}
  static void (*FinishCallback)();
};