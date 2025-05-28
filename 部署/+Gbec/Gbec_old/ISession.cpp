#include "ISession.h"
#include "SerialIO.h"
//静态成员必须类外定义
std::vector<const ITrial *> ISession::TrialQueue;
uint16_t ISession::TrialsDone;
uint16_t ISession::TrialsRestored;
void (*ISession::FinishCallback)();
uint32_t TimeShift;
void ISession::RunAsync() {
#pragma pack(push, 1)
  struct TrialSignal {
    UID Signal = Signal_TrialStart;
    uint16_t TrialIndex;
    UID TrialUID;
    TrialSignal(uint16_t TrialIndex, UID TrialUID)
      : TrialIndex(TrialIndex), TrialUID(TrialUID) {}
  };
#pragma pack(pop)
  while (TrialsDone < TrialQueue.size()) {
    SerialWrite(TrialSignal(TrialsRestored + TrialsDone, TrialQueue[TrialsDone]->MyUID));
    if (TrialQueue[TrialsDone++]->Start(RunAsync))
      return;
  }
  FinishCallback();
}