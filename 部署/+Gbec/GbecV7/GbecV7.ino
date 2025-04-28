#include "ExperimentDesign.hpp"
#include <sstream>
#include <map>
#include <vector>
std::set<Process*> Process::Existing;
template <typename T>
struct ProcessConstructors;
template <typename... Pairs>
struct ProcessConstructors<std::tuple<Pairs...>> {
	static std::unordered_map<UID, Process* (*)()> value;
};
template <typename... Pairs>
std::unordered_map<UID, Process* (*)()> ProcessConstructors<std::tuple<Pairs...>>::value{ {Pairs::ID, Process::New<Pairs::StepType>}... };

template <typename T>
inline void BindFunctionToPort(T&& Function, UID Port) {
	Async_stream_IO::BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}

ArchUrng Urng;
std::move_only_function<void() const> const NullCallback = []() {};
void setup() {
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]() { return static_cast<uint8_t>(sizeof(size_t)); }, UID::PortA_PointerSize);
	Async_stream_IO::Listen([](const std::move_only_function<void(void* Message, uint8_t Size) const>& MessageReader, uint8_t MessageSize) {
		static std::vector<char> Message;
		Message.resize(MessageSize);
		MessageReader(Message.data(), MessageSize);
#pragma pack(push, 1)
		struct RestoreHeader {
			UID StepID;
			struct TrialDone {
				UID TrialID;
				uint16_t NumDone;
			}TrialsDone[];
		};
		struct RepeatHeader {
			UID StepID;
			uint16_t Times;
		};
#pragma pack(pop)
		switch (MessageSize) {
			case sizeof(UID) :
			{
				const auto Iterator = ProcessConstructors<PublicSteps>::value.find(*reinterpret_cast<UID*>(Message.data()));
				if (Iterator == ProcessConstructors<PublicSteps>::value.end())
					Async_stream_IO::Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Signal));
				else {
					Process* const P = Iterator->second();
					Async_stream_IO::Send(P, static_cast<uint8_t>(P->Start() ? UID::PortC_ProcessFinished : UID::PortC_ProcessStarted));
				}
			}
			break;
			case sizeof(RepeatHeader) :
			{
				RepeatHeader const* const Arguments = reinterpret_cast<RepeatHeader*>(Message.data());
				const auto Iterator = ProcessConstructors<PublicSteps>::value.find(Arguments->StepID);
				if (Iterator == ProcessConstructors<PublicSteps>::value.end())
					Async_stream_IO::Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Signal));
				else {
					Process* const P = Iterator->second();
					Async_stream_IO::Send(P, static_cast<uint8_t>(P->Repeat(Arguments->Times) ? UID::PortC_ProcessFinished : UID::PortC_ProcessStarted));
				}
			}
			break;
			default:
			{
				RestoreHeader const* const Arguments = reinterpret_cast<RestoreHeader*>(Message.data());
				const auto Iterator = ProcessConstructors<PublicSteps>::value.find(Arguments->StepID);
				if (Iterator == ProcessConstructors<PublicSteps>::value.end())
					Async_stream_IO::Send(UID::Exception_InvalidProcess, static_cast<uint8_t>(UID::PortC_Signal));
				else {
					Process* const P = Iterator->second();
					uint8_t const NumTrialsDone = (MessageSize - sizeof(UID)) / sizeof(RestoreHeader::TrialDone);
					std::unordered_map<UID, uint16_t> TrialsDone;
					TrialsDone.reserve(NumTrialsDone);
					for (uint8_t i = 0; i < NumTrialsDone; ++i)
						TrialsDone[Arguments->TrialsDone[i].TrialID] = Arguments->TrialsDone[i].NumDone;
					Async_stream_IO::Send(P, static_cast<uint8_t>(P->Restore(std::move(TrialsDone)) ? UID::PortC_ProcessFinished : UID::PortC_ProcessStarted));
				}
			}
		} }, static_cast<uint8_t>(UID::PortA_CreateProcess));
		BindFunctionToPort([](Process* P) {
			if (Process::Existing.contains(P)) {
				noInterrupts();
				P->Pause();
				interrupts();
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidProcess; }, UID::PortA_PauseProcess);
		BindFunctionToPort([](Process* P) {
			if (Process::Existing.contains(P)) {
				noInterrupts();
				P->Continue();
				interrupts();
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidProcess; }, UID::PortA_ContinueProcess);
		BindFunctionToPort([](Process* P) {
			if (Process::Existing.contains(P)) {
				noInterrupts();
				P->Abort();
				interrupts();
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidProcess; }, UID::PortA_AbortProcess);
		BindFunctionToPort([](Process* P, uint8_t ReceivePort) {
			if (Process::Existing.contains(P)) {
				std::ostringstream OutStream;
				P->WriteInfo(OutStream);
				std::string const Message = OutStream.str();
				Async_stream_IO::Send(Message.data(), Message.size(), ReceivePort);
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidProcess; }, UID::PortA_GetInformation);
		BindFunctionToPort([](Process* P) {
			if (Process::Existing.erase(P)) {
				noInterrupts();
				delete P;
				interrupts();
				return UID::Exception_Success;
			}
			else
				return UID::Exception_InvalidProcess; }, UID::PortA_DeleteProcess);
		BindFunctionToPort([](uint8_t ToPort) { Async_stream_IO::Send([](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender) {
			const size_t MessageSize = sizeof(Step*) * RootSteps.size() + sizeof(uint8_t);
			std::unique_ptr<uint8_t[]>Message = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
			Message[0] = RootSteps.size();
			std::copy(RootSteps.cbegin(), RootSteps.cend(), reinterpret_cast<Step**>(Message.get() + 1));
			MessageSender(Message.get(), MessageSize); }, ToPort); },
			UID::Port_AllObjects);
#ifdef ARDUINO_ARCH_AVR
		Async_stream_IO::RemoteInvoke(static_cast<uint8_t>(UID::Port_RandomSeed), [](Async_stream_IO::Exception, uint32_t RandomSeed) { std::ArduinoUrng::seed(RandomSeed); });
#endif
}
std::set<std::move_only_function<void() const> const*> _PendingInterrupts;
void loop() {
	noInterrupts();
	for (auto M : _PendingInterrupts)
		M->operator()();
	_PendingInterrupts.clear();
	interrupts();
	Async_stream_IO::ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>