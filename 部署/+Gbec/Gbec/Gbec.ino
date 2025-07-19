#include "ExperimentDesign.hpp"
#include <sstream>
#include <map>
#include <vector>
#include <memory>
Async_stream_IO::AsyncStream SerialStream{ Serial };
struct Process {
	template<typename StepType>
	static Process* New() {
		Process* NewProcess = new Process();
		NewProcess->Content = std::make_unique<StepType>(NewProcess->ChildCallback, NewProcess);
		return NewProcess;
	}
	UID Start() {
		Quick_digital_IO_interrupt::InterruptGuard const _;
		if (State != UID::State_Idle)
			return UID::Exception_ProcessNotIdle;
		RepeatLeft = 0;
		if (Content->Start())
			return UID::Exception_ProcessFinished;
		State = UID::State_Running;
		return UID::Exception_Success;
	}
	UID Restore(std::unordered_map<UID, uint16_t>&& TD) {
		Quick_digital_IO_interrupt::InterruptGuard const _;
		if (State != UID::State_Idle)
			return UID::Exception_ProcessNotIdle;
		RepeatLeft = 0;
		if (Content->Restore(TrialsDone = std::move(TD)))
			return UID::Exception_ProcessFinished;
		State = UID::State_Running;
		return UID::Exception_Success;
	}
	UID Repeat(uint16_t Times) {
		Quick_digital_IO_interrupt::InterruptGuard const _;
		if (State != UID::State_Idle)
			return UID::Exception_ProcessNotIdle;
		for (;;) {
			if (!Content->Start()) {
				RepeatLeft = Times - 1;
				State = UID::State_Running;
				return UID::Exception_Success;
			}
			if (!--Times) {
				State = UID::State_Idle;
				return UID::Exception_ProcessFinished;
			}
		}
	}
	UID Pause() {
		Quick_digital_IO_interrupt::InterruptGuard const _;
		if (State != UID::State_Running)
			return UID::Exception_ProcessNotRunning;
		State = UID::State_Paused;
		Content->Pause();
		return UID::Exception_Success;
	}
	UID Continue() {
		Quick_digital_IO_interrupt::InterruptGuard const _;
		if (State != UID::State_Paused)
			return UID::Exception_ProcessNotPaused;
		State = UID::State_Running;
		Content->Continue();
		return UID::Exception_Success;
	}
	UID Abort() {
		Quick_digital_IO_interrupt::InterruptGuard const _;
		if (State == UID::State_Idle)
			return UID::Exception_ProcessNotRunning;
		State = UID::State_Aborted;
		Content->Abort();
		return UID::Exception_Success;
	}
	void WriteInfo(std::ostream& OutStream) const {
		Content->WriteInfoD(OutStream);
	}
	static std::set<Process*> Existing;
	UID State = UID::State_Idle;
	~Process() {
		Abort();
	}

protected:
	std::move_only_function<void() const> const ChildCallback{ [this]() {
		while (RepeatLeft) {
			RepeatLeft--;
			if (!Content->Start())
				return;
		}
		State = UID::State_Idle;
		SerialStream.Send(this, static_cast<uint8_t>(UID::PortC_ProcessFinished));
	} };
	std::unique_ptr<Step> Content;
	std::unordered_map<UID, uint16_t> TrialsDone;
	uint16_t RepeatLeft;
	Process() {
		Existing.insert(this);
	}
};
std::set<Process*> Process::Existing;
template<typename T>
struct ProcessConstructors;
template<typename... Pairs>
struct ProcessConstructors<std::tuple<Pairs...>> {
	static std::unordered_map<UID, Process* (*)()> value;
};
template<typename... Pairs>
std::unordered_map<UID, Process* (*)()> ProcessConstructors<std::tuple<Pairs...>>::value{ { Pairs::ID, Process::New<typename Pairs::StepType> }... };

template<typename T>
inline void BindFunctionToPort(T&& Function, UID Port) {
	SerialStream.BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}
template<typename T>
inline void SerialListen(T&& Callback, UID Port) {
	SerialStream.Listen(std::forward<T>(Callback), static_cast<uint8_t>(Port));
}
template<typename T>
inline void SerialRead(T& Object) {
	Serial.readBytes(reinterpret_cast<char*>(&Object), sizeof(T));
}
bool CommonListenersHeader(uint8_t MessageSize, uint8_t& ReturnPort, Process*& P) {
	if (MessageSize < sizeof(ReturnPort) + sizeof(P))
		return true;
	SerialRead(ReturnPort);
	SerialRead(P);
	if (Process::Existing.contains(P))
		return false;
	else {
		SerialStream.Send(UID::Exception_InvalidProcess, ReturnPort);
		return true;
	}
}

ArchUrng Urng;
std::move_only_function<void() const> const NullCallback{ []() {} };
void setup() {
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]() {
		return static_cast<uint8_t>(sizeof(size_t));
	},
	                   UID::PortA_PointerSize);
	BindFunctionToPort([](UID StepID) -> Process* {
		const auto Iterator = ProcessConstructors<PublicSteps>::value.find(StepID);
		if (Iterator == ProcessConstructors<PublicSteps>::value.end()) {
			SerialStream.Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Exception));
			return nullptr;
		} else
			return Iterator->second();
	},
	                   UID::PortA_CreateProcess);
	BindFunctionToPort([](Process* P) {
		return Process::Existing.contains(P) ? P->Start() : UID::Exception_InvalidProcess;
	},
	                   UID::PortA_StartProcess);
	BindFunctionToPort([](Process* P, uint8_t Times) {
		return Process::Existing.contains(P) ? P->Repeat(Times) : UID::Exception_InvalidProcess;
	},
	                   UID::PortA_RepeatProcess);
	SerialListen([](uint8_t MessageSize) {
		uint8_t ReturnPort;
		Process* P;
		if (CommonListenersHeader(MessageSize, ReturnPort, P))
			return;
		MessageSize = (MessageSize - (sizeof(uint8_t) + sizeof(Process*))) / (sizeof(UID) + sizeof(uint16_t));
		std::unordered_map<UID, uint16_t> TrialsDone;
		TrialsDone.reserve(MessageSize);
		for (uint8_t i = 0; i < MessageSize; ++i) {
			UID TrialID;
			uint16_t NumDone;
			Serial.readBytes(reinterpret_cast<char*>(&TrialID), sizeof(UID));
			Serial.readBytes(reinterpret_cast<char*>(&NumDone), sizeof(uint16_t));
			TrialsDone[TrialID] = NumDone;
		}
		SerialStream.Send(P->Restore(std::move(TrialsDone)), ReturnPort);
	},
	             UID::PortA_RestoreProcess);
	BindFunctionToPort([](Process* P) {
		return Process::Existing.contains(P) ? P->Pause() : UID::Exception_InvalidProcess;
	},
	                   UID::PortA_PauseProcess);
	BindFunctionToPort([](Process* P) {
		return Process::Existing.contains(P) ? P->Continue() : UID::Exception_InvalidProcess;
	},
	                   UID::PortA_ContinueProcess);
	BindFunctionToPort([](Process* P) {
		return Process::Existing.contains(P) ? P->Abort() : UID::Exception_InvalidProcess;
	},
	                   UID::PortA_AbortProcess);
	SerialListen([](uint8_t MessageSize) {
		uint8_t ReturnPort;
		Process* P;
		if (CommonListenersHeader(MessageSize, ReturnPort, P))
			return;
		std::ostringstream OutStream;
		P->WriteInfo(OutStream);
		std::string const Info = OutStream.str();
		SerialStream.Send(Info.data(), Info.size(), ReturnPort);
	},
	             UID::PortA_GetInformation);
	BindFunctionToPort([](Process* P) {
		if (Process::Existing.erase(P)) {
			delete P;
			return UID::Exception_Success;
		} else
			return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_DeleteProcess);
	SerialListen([](uint8_t MessageSize) {
		if (MessageSize < sizeof(uint8_t))
			return;
		uint8_t ToPort;
		SerialRead(ToPort);
		Async_stream_IO::SendSession const Session{ sizeof(Process*) * Process::Existing.size(), ToPort, Serial };
		for (Process* const P : Process::Existing)
			Serial.write(reinterpret_cast<const char*>(&P), sizeof(P));
	},
	             UID::PortA_AllProcesses);
#ifdef ARDUINO_ARCH_AVR
	SerialStream.RemoteInvoke(static_cast<uint8_t>(UID::PortC_RandomSeed), [](Async_stream_IO::Exception, uint32_t RandomSeed) {
		std::ArduinoUrng::seed(RandomSeed);
	});
#endif
	BindFunctionToPort([]() {
		return true;
	},
	                   UID::PortA_IsReady);
}
std::set<std::move_only_function<void() const> const*> _PendingInterrupts;
void loop() {
	noInterrupts();  //不交换到本地，直接禁止中断执行，以控制中断频率
	for (auto M : _PendingInterrupts)
		M->operator()();
	_PendingInterrupts.clear();
	interrupts();
	SerialStream.ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>