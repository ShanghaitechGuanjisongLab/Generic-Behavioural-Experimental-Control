#include "ExperimentDesign.hpp"
#include <sstream>
#include <map>
#include <vector>
Async_stream_IO::AsyncStream SerialStream{ Serial };
struct Process {
	template<typename StepType>
	static Process* New() {
		Process* NewProcess = new Process();
		NewProcess->Content = std::make_unique<StepType>(NewProcess->ChildCallback, NewProcess);
		Existing.insert(NewProcess);
		return NewProcess;
	}
	void Start() {
		RepeatLeft = 0;
		State = Content->Start() ? UID::State_Finished : UID::State_Running;
	}
	void Restore(std::unordered_map<UID, uint16_t>&& TD) {
		State = Content->Restore(TrialsDone = std::move(TD)) ? UID::State_Finished : UID::State_Running;
	}
	void Repeat(uint16_t Times) {
		for (;;) {
			if (!Content->Start()) {
				RepeatLeft = Times - 1;
				State = UID::State_Running;
				break;
			}
			if (!--Times) {
				State = UID::State_Finished;
				break;
			}
		}
	}
	void Pause() {
		State = UID::State_Paused;
		Content->Pause();
	}
	void Continue() {
		State = UID::State_Running;
		Content->Continue();
	}
	void Abort() {
		State = UID::State_Aborted;
		Content->Abort();
	}
	void WriteInfo(std::ostream& OutStream) const {
		Content->WriteInfoD(OutStream);
	}
	static std::set<Process*> Existing;

protected:
	std::move_only_function<void() const> const ChildCallback{ [this]() {
		while (RepeatLeft) {
			RepeatLeft--;
			if (!Content->Start())
				return;
		}
		State = UID::State_Finished;
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
	SerialStream.Listen([](const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) {
		static std::vector<char> Message;
		Message.resize(MessageSize);
		MessageReader(Message.data(), MessageSize);
#pragma pack(push, 1)
		struct RestoreHeader {
			UID StepID;
			struct TrialDone {
				UID TrialID;
				uint16_t NumDone;
			} TrialsDone[];
		};
		struct RepeatHeader {
			UID StepID;
			uint16_t Times;
		};
#pragma pack(pop)
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
		static_cast<uint8_t>(UID::PortA_CreateProcess));
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