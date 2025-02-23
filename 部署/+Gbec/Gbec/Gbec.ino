#include "ExperimentDesign.h"
UID State = State_SessionInvalid;
std::string InfoBuffer;
void SessionFinish() {
  Serial.write(State = State_SessionFinished);
}
void setup() {
  Serial.setTimeout(-1);
  Serial.begin(9600);
  // PC端无法确认何时初始化完毕，不能提前发送信号，必须等待Arduino端宣布初始化完毕
  PinSetup();
  ISession::FinishCallback = SessionFinish;
  Serial.write(Signal_SerialReady);
  std::ArduinoUrng::seed(SerialRead<uint32_t>());
}
const ISession* CurrentSession;
void Start() {
  const UID SessionUID = SerialRead<UID>();
  if (State == State_SessionRunning || State == State_SessionPaused) {
    Serial.write(State);
    return;
  }
  const auto Query = SessionMap.find(SessionUID);
  if (Query == SessionMap.end()) {
    Serial.write(Signal_NoSuchSession);
    return;
  }
  TimeShift = 0;
  CurrentSession = (*Query).second;
  State = State_SessionRunning;
  Serial.write(Signal_SessionStarted);
  CurrentSession->Start();
}
void Pause() {
  if (State == State_SessionRunning) {
    State = State_SessionPaused;
    CurrentSession->Pause();
  }
  Serial.write(State);
}
void Continue() {
  if (State == State_SessionPaused) {
    Serial.write(Signal_SessionContinue);
    State = State_SessionRunning;
    CurrentSession->Continue();
  }
  else
    Serial.write(State);
}
void Abort() {
  if (State == State_SessionPaused || State == State_SessionRunning) {
    CurrentSession->Abort();
    State = State_SessionAborted;
  }
  Serial.write(State);
}
void Peek() {
  Serial.write(State);
}
void GetInfo() {
  const UID SessionUID = SerialRead<UID>();
  if (State == State_SessionRunning)
    Serial.write(State);
  else if (SessionUID == Session_Current) {
    if (State == State_SessionInvalid)
      Serial.write(State);
    else {
      Serial.write(Signal_InfoStart);
      CurrentSession->WriteInfo();
    }
  }
  else {
    const auto Query = SessionMap.find(SessionUID);
    if (Query == SessionMap.end())
      Serial.write(Signal_NoSuchSession);
    else {
      Serial.write(Signal_InfoStart);
      (*Query).second->WriteInfo();
    }
  }
}
void Restore() {
#pragma pack(push, 1)
  struct RestoreCommand {
    uint32_t TimeShift;
    UID SessionUID;
    uint8_t NumDistinctTrials;
  };
  const RestoreCommand RC = SerialRead<RestoreCommand>();
  const std::unique_ptr<RestoreInfo[]> RIs = std::make_unique<RestoreInfo[]>(RC.NumDistinctTrials);
  SerialRead(RIs.get(), RC.NumDistinctTrials);
  if (State == State_SessionRunning || State == State_SessionPaused) {
    SerialWrite(State);
    return;
  }
  const auto Query = SessionMap.find(RC.SessionUID);
  if (Query == SessionMap.end()) {
    SerialWrite(Signal_NoSuchSession);
    return;
  }
  TimeShift = RC.TimeShift;
  CurrentSession = (*Query).second;
  State = State_SessionRunning;
  Serial.write(Signal_SessionRestored);
  CurrentSession->Restore(RC.NumDistinctTrials, RIs.get());
}
ITest* CurrentTest = nullptr;
void TestStart() {
  struct TestInfo {
    UID TestUID;
    uint16_t TestTimes;
  };
#pragma pack(pop)
  const TestInfo TI = SerialRead<TestInfo>();
  if (State == State_SessionRunning || State == State_SessionPaused) {
    Serial.write(State);
    return;
  }
  const auto Query = TestMap.find(TI.TestUID);
  if (Query == TestMap.end()) {
    Serial.write(Signal_NoSuchTest);
    return;
  }
  CurrentTest = (*Query).second;
  if (CurrentTest->Start(TI.TestTimes)) {
    CurrentTest = nullptr;
    Serial.write(Signal_TestStartedAutoStop);
  }
  else
    Serial.write(Signal_TestStartedManualStop);
}
void TestStop() {
  const UID TestUID = SerialRead<UID>();
  if (TestUID == Test_Last)
    if (CurrentTest) {
      CurrentTest->Stop();
      CurrentTest = nullptr;
      Serial.write(State == State_SessionRunning ? State_SessionRunning : Signal_TestStopped);
    }
    else
      Serial.write(Signal_NoLastTest);
  else {
    const auto Query = TestMap.find(TestUID);
    if (Query == TestMap.end())
      Serial.write(Signal_NoSuchTest);
    else {
      const ITest* const TestToStop = (*Query).second;
      TestToStop->Stop();
      if (TestToStop == CurrentTest)
        CurrentTest = nullptr;
      Serial.write(State == State_SessionRunning ? State_SessionRunning : Signal_TestStopped);
    }
  }
}
void IsReady() {
  Serial.write(Signal_SerialReady);
}
void loop() {
  constexpr void (*APIs[])() = {
    Start,
    Pause,
    Continue,
    Abort,
    Peek,
    GetInfo,
    Restore,
    TestStart,
    TestStop,
    IsReady,
  };
  const uint8_t API = SerialRead<uint8_t>();
  if (API < std::extent_v<decltype(APIs)>_CSL_Parentheses11) {
    Serial.write(Signal_ApiFound);
    APIs[API]();
  }
  else
    Serial.write(Signal_ApiInvalid);
}
#if __cplusplus < 201703L
//C++17之前，即使是 static constexpr 成员，也必须在类外定义
template<typename... SubSteps>
constexpr IStep* IndividualThreadStep<SubSteps...>::StepPointers[];
template<typename TTrial, typename TNumber, typename... TrialThenNumber>
constexpr typename TrialNumberSplit<TTrial, TNumber, TrialThenNumber...>::Numbers_t TrialNumberSplit<TTrial, TNumber, TrialThenNumber...>::Numbers;
template<typename TTrial, typename TNumber>
constexpr typename TrialNumberSplit<TTrial, TNumber>::Numbers_t TrialNumberSplit<TTrial, TNumber>::Numbers;
constexpr std::ArduinoUrng ISession::Urng;
#endif
#include<TimersOneForAll_Define.hpp>