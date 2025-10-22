#include "Predefined.hpp"
// SAM编译器bug，此定义必须放前面否则找不到
#pragma pack(push, 1)
struct GbecHeader {
	Async_stream_IO::Port RemotePort;
	Process *P;
};
struct ModuleStartReturn {
	UID GbecException;
	uint16_t NumTrials;
};
#pragma pack(pop)
std::queue<std::move_only_function<void()> *> PinListener::PendingCallbacks;
std::unordered_map<uint8_t, std::set<std::move_only_function<void()> *>> PinListener::Listening;
std::unordered_map<uint8_t, std::set<std::move_only_function<void()> *>> PinListener::Resting;
std::move_only_function<void()> Module::_EmptyCallback{ []() {} };
Async_stream_IO::AsyncStream SerialStream;
extern std::unordered_map<UID, uint16_t (*)(Process *)> SessionMap;
static std::set<Process *> ExistingProcesses;
UID const Delay<Infinite, Infinite>::ID = UID::Module_Delay;

template<typename T>
inline void BindFunctionToPort(T &&Function, UID Port) {
	SerialStream.BindFunctionToPort(std::forward<T>(Function), static_cast<Async_stream_IO::Port>(Port));
}
template<typename T>
inline void SerialListen(T &&Callback, UID Port) {
	SerialStream.Listen(std::forward<T>(Callback), static_cast<Async_stream_IO::Port>(Port));
}
bool CommonListenersHeader(Async_stream_IO::MessageSize &MessageSize, GbecHeader &Header) {
	if (MessageSize < sizeof(Header))
		return true;
	SerialStream >> Header;
	MessageSize -= sizeof(Header);
	if (ExistingProcesses.contains(Header.P))
		return false;
	SerialStream.Send(UID::Exception_InvalidProcess, Header.RemotePort);
	SerialStream.Skip(MessageSize);
	return true;
}

bool Debug = false;
void setup() {
	pinMode(6, OUTPUT);
	Serial.begin(9600);
	Serial.setTimeout(-1);
	BindFunctionToPort([]() {
		return static_cast<uint8_t>(sizeof(void const *));
	},
	                   UID::PortA_PointerSize);
	BindFunctionToPort([]() {
		return true;
	},
	                   UID::PortA_IsReady);
#ifdef ARDUINO_ARCH_AVR
	BindFunctionToPort(std::ArduinoUrng::seed,
	                   UID::PortA_RandomSeed);
#endif
	BindFunctionToPort([]() {
		Process *P = new Process;
		ExistingProcesses.insert(P);
		return P;
	},
	                   UID::PortA_CreateProcess);
	BindFunctionToPort([](Process *P) {
		if (ExistingProcesses.erase(P)) {
			delete P;
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_DeleteProcess);
	SerialListen([](Async_stream_IO::MessageSize MessageSize) {
		GbecHeader Header;
		if (CommonListenersHeader(MessageSize, Header))
			return;
		auto const Iterator = SessionMap.find(SerialStream.Read<UID>());
		MessageSize -= sizeof(UID);
		if (Iterator == SessionMap.end()) {
			SerialStream.Skip(MessageSize);
			SerialStream.Send(UID::Exception_InvalidModule, Header.RemotePort);
			return;
		}
		uint16_t Times;
		switch (MessageSize) {
			case 0:
				Times = 1;
				break;
			case sizeof(uint16_t):
				Times = SerialStream.Read<uint16_t>();
				break;
			default:
				SerialStream.Skip(MessageSize);
				SerialStream.Send(UID::Exception_BrokenStartArguments, Header.RemotePort);
				return;
		}
		Header.P->TrialsDone.clear();
		SerialStream.Send(ModuleStartReturn{ UID::Exception_Success, Iterator->second(Header.P) }, Header.RemotePort);
		if (!Header.P->Start(Times))
			SerialStream.AsyncInvoke(static_cast<Async_stream_IO::Port>(UID::PortC_ProcessFinished), Header.P);
	},
	             UID::PortA_StartModule);
	SerialListen([](Async_stream_IO::MessageSize MessageSize) {
		GbecHeader Header;
		if (CommonListenersHeader(MessageSize, Header))
			return;
		auto const Iterator = SessionMap.find(SerialStream.Read<UID>());
		MessageSize -= sizeof(UID);
		if (Iterator == SessionMap.end()) {
			SerialStream.Skip(MessageSize);
			SerialStream.Send(UID::Exception_InvalidModule, Header.RemotePort);
			return;
		}
		MessageSize /= (sizeof(UID) + sizeof(uint16_t));
		std::unordered_map<UID, uint16_t> &TrialsDone = Header.P->TrialsDone;

		Iterator->second(Header.P);
		//必须先载入模块，然后再设置TrialsDone，因为载入模块会清空TrialsDone
		for (uint8_t i = 0; i < MessageSize; ++i) {
			UID const TrialID = SerialStream.Read<UID>();
			TrialsDone[TrialID] = SerialStream.Read<uint16_t>();
		}

		SerialStream.Send(UID::Exception_Success, Header.RemotePort);
		if (!Header.P->Start(1))
			SerialStream.AsyncInvoke(static_cast<Async_stream_IO::Port>(UID::PortC_ProcessFinished), Header.P);
	},
	             UID::PortA_RestoreModule);
	BindFunctionToPort([](Process *P) {
		if (ExistingProcesses.contains(P)) {
			P->Pause();
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_PauseProcess);
	BindFunctionToPort([](Process *P) {
		if (ExistingProcesses.contains(P)) {
			P->Continue();
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_ContinueProcess);
	BindFunctionToPort([](Process *P) {
		if (ExistingProcesses.contains(P)) {
			P->Abort();
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_AbortProcess);
	SerialListen([](Async_stream_IO::MessageSize MessageSize) {
		GbecHeader Header;
		if (CommonListenersHeader(MessageSize, Header))
			return;
		Header.P->SendInfo(Header.RemotePort);
	},
	             UID::PortA_GetInformation);
	SerialListen([](Async_stream_IO::MessageSize MessageSize) {
		if (MessageSize < sizeof(Async_stream_IO::Port))
			return;
		Async_stream_IO::InterruptGuard const Token = SerialStream.BeginSend(sizeof(Process *) * ExistingProcesses.size(), SerialStream.Read<Async_stream_IO::Port>());
		for (Process *const P : ExistingProcesses)
			SerialStream << P;
	},
	             UID::PortA_AllProcesses);
	BindFunctionToPort([](Process *P) {
		return ExistingProcesses.contains(P);
	},
	                   UID::PortA_ProcessValid);
	SerialStream.Send(nullptr, 0, static_cast<Async_stream_IO::Port>(UID::PortC_ImReady));
}
void loop() {
	PinListener::ClearPending();
	SerialStream.ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>