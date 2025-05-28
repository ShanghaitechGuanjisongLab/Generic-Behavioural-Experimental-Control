#pragma once
#include "IStepTrial.h"
#include <vector>
#include <random>
#pragma pack(push, 1)
struct RestoreInfo {
  UID TrialUID;
  uint16_t NumDone;
};
#pragma pack(pop)
struct ISession {
  UID MyUID;
  virtual void Start() const = 0;
  void Pause() const {
    TrialQueue[TrialsDone - 1]->Pause();
  }
  void Continue() const {
    TrialQueue[TrialsDone - 1]->Continue();
  }
  void Abort() const {
    TrialQueue[TrialsDone - 1]->Abort();
  }
  virtual void WriteInfo() const = 0;
  virtual void Restore(uint8_t NumDistinctTrials, const RestoreInfo *RI) const = 0;
  static void (*FinishCallback)();

protected:
  static std::vector<const ITrial *> TrialQueue;
  static uint16_t TrialsDone;
  static uint16_t TrialsRestored;
  static void RunAsync();
  static constexpr std::ArduinoUrng Urng = std::ArduinoUrng();
  constexpr ISession(UID MyUID)
    : MyUID(MyUID) {}
};