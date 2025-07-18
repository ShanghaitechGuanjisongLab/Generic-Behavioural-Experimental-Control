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
		Existing.insert(NewProcess);
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
	Process() {}
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

ArchUrng Urng;
std::move_only_function<void() const> const NullCallback{ []() {} };
void setup() {
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]() {
		return static_cast<uint8_t>(sizeof(size_t));
		},
		UID::PortA_PointerSize);
	BindFunctionToPort([](UID StepID)->Process* {
		const auto Iterator = ProcessConstructors<PublicSteps>::value.find(StepID);
		if (Iterator == ProcessConstructors<PublicSteps>::value.end()) {
			SerialStream.Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Exception));
			return nullptr;
		}
		else
			return Iterator->second();
		},
		UID::PortA_CreateProcess);
	BindFunctionToPort([](Process* P) {
		return Process::Existing.contains(P) ? P->Start() : UID::Exception_InvalidProcess;
		}, UID::PortA_StartProcess);
	BindFunctionToPort([](Process* P, uint8_t Times) {
		return Process::Existing.contains(P) ? P->Repeat(Times) : UID::Exception_InvalidProcess;
		}, UID::PortA_RepeatProcess);
	SerialStream.Listen([](uint8_t MessageSize) {
#pragma pack(push, 1)
		struct RestoreHeader {
			uint8_t ReturnPort;
			Process* P;
			struct TrialDone {
				UID TrialID;
				uint16_t NumDone;
			} TrialsDone[];
		};
#pragma pack(pop)
		std::unique_ptr<char[]>Buffer = std::make_unique_for_overwrite<char[]>(MessageSize);
		RestoreHeader* RH = reinterpret_cast<RestoreHeader*>(Buffer.get());
		Serial.readBytes(reinterpret_cast<char*>(RH), static_cast<size_t>(MessageSize));

		Process* P;
		switch (MessageSize) {
			case sizeof(UID) :
			{
				const auto Iterator = ProcessConstructors<PublicSteps>::value.find(*reinterpret_cast<UID*>(Message.data()));
				if (Iterator == ProcessConstructors<PublicSteps>::value.end()) {
					SerialStream.Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Exception));
					return;
				}
				else {
					P = Iterator->second();
					P->Start();
				}
			}
			break;
			case sizeof(RepeatHeader) :
			{
				RepeatHeader const* const Arguments = reinterpret_cast<RepeatHeader*>(Message.data());
				const auto Iterator = ProcessConstructors<PublicSteps>::value.find(Arguments->StepID);
				if (Iterator == ProcessConstructors<PublicSteps>::value.end()) {
					SerialStream.Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Exception));
					return;
				}
				else {
					P = Iterator->second();
					P->Repeat(Arguments->Times);
				}
			}
			break;
			default:
			{
				RestoreHeader const* const Arguments = reinterpret_cast<RestoreHeader*>(Message.data());
				const auto Iterator = ProcessConstructors<PublicSteps>::value.find(Arguments->StepID);
				if (Iterator == ProcessConstructors<PublicSteps>::value.end()) {
					SerialStream.Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Exception));
					return;
				}
				else {
					P = Iterator->second();
					uint8_t const NumTrialsDone = (MessageSize - sizeof(UID)) / sizeof(RestoreHeader::TrialDone);
					std::unordered_map<UID, uint16_t> TrialsDone;
					TrialsDone.reserve(NumTrialsDone);
					for (uint8_t i = 0; i < NumTrialsDone; ++i)
						TrialsDone[Arguments->TrialsDone[i].TrialID] = Arguments->TrialsDone[i].NumDone;
					P->Restore(std::move(TrialsDone));
				}
			}
			SerialStream.Send(P, static_cast<uint8_t>(UID::PortC_ProcessStarted));//每个新进程必须发送一个开始信号，以便PC端保存指针
			if (P->State == UID::State_Finished)
				SerialStream.Send(P, static_cast<uint8_t>(UID::PortC_ProcessFinished));
		}
		},
		static_cast<uint8_t>(UID::PortA_RestoreProcess));
	BindFunctionToPort([](Process* P) {
		if (Process::Existing.contains(P)) {
			noInterrupts();
			if (P->State == UID::State_Running) {
				P->Pause();
				interrupts();
				return UID::Exception_Success;
			}
			else {
				interrupts();
				return UID::Exception_ProcessNotRunning;
			}
		}
		else
			return UID::Exception_InvalidProcess;
		},
		UID::PortA_PauseProcess);
	BindFunctionToPort([](Process* P) {
		if (Process::Existing.contains(P)) {
			noInterrupts();
			if (P->State == UID::State_Paused) {
				P->Continue();
				interrupts();
				return UID::Exception_Success;
			}
			else {
				interrupts();
				return UID::Exception_ProcessNotPaused;
			}
		}
		else
			return UID::Exception_InvalidProcess;
		},
		UID::PortA_ContinueProcess);
	BindFunctionToPort([](Process* P) {
		if (Process::Existing.contains(P)) {
			noInterrupts();
			if (P->State == UID::State_Paused || P->State == UID::State_Running) {
				P->Continue();
				interrupts();
				return UID::Exception_Success;
			}
			else {
				interrupts();
				return UID::Exception_ProcessNotRunning;
			}
		}
		else
			return UID::Exception_InvalidProcess;
		},
		UID::PortA_AbortProcess);
	BindFunctionToPort([](Process* P, uint8_t ReceivePort) {
		if (Process::Existing.contains(P)) {
			std::ostringstream OutStream;
			P->WriteInfo(OutStream);
			std::string const Message = OutStream.str();
			SerialStream.Send(Message.data(), Message.size(), ReceivePort);
			return UID::Exception_Success;
		}
		else
			return UID::Exception_InvalidProcess;
		},
		UID::PortA_GetInformation);
	BindFunctionToPort([](Process* P) {
		if (Process::Existing.erase(P)) {
			noInterrupts();
			delete P;
			interrupts();
			return UID::Exception_Success;
		}
		else
			return UID::Exception_InvalidProcess;
		},
		UID::PortA_DeleteProcess);
	SerialStream.Listen([](const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) {
		uint8_t ToPort;
		for (uint8_t M = 0; M < MessageSize; M++)
			MessageReader(&ToPort, 1);
		Async_stream_IO::SendSession const Session{ sizeof(Process*) * Process::Existing.size() + sizeof(uint8_t), ToPort, Serial };
		Serial.write(Process::Existing.size());
		for (Process* const P : Process::Existing)
			Serial.write(reinterpret_cast<const char*>(&P), sizeof(P));
		},
		static_cast<uint8_t>(UID::PortA_AllProcesses));
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
	noInterrupts();
	for (auto M : _PendingInterrupts)
		M->operator()();
	_PendingInterrupts.clear();
	interrupts();
	SerialStream.ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>