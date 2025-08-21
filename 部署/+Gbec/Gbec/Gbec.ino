#include "Predefined.hpp"
// SAM编译器bug，此定义必须放前面否则找不到
#pragma pack(push, 1)
struct GbecHeader {
	uint8_t RemotePort;
	Process* P;
};
struct ModuleStartReturn {
	UID Exception;
	uint16_t NumTrials;
};
#pragma pack(pop)
std::queue<std::move_only_function<void() const> const*> PinListener::PendingCallbacks;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> PinListener::Listening;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> PinListener::Resting;
std::move_only_function<void() const> const Module::_EmptyCallback{ []() {} };
Async_stream_IO::AsyncStream SerialStream{ Serial };
extern std::unordered_map<UID, bool (*)(Process*, uint16_t, uint16_t&)> SessionMap;
static std::set<Process*> ExistingProcesses;
UID const Delay<InfiniteDuration>::ID = UID::Module_Delay;

template<typename T>
inline void BindFunctionToPort(T&& Function, UID Port) {
	SerialStream.BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}
template<typename T>
inline void SerialListen(T&& Callback, UID Port) {
	SerialStream.Listen(std::forward<T>(Callback), static_cast<uint8_t>(Port));
}
bool CommonListenersHeader(uint8_t& MessageSize, GbecHeader& Header) {
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

void setup() {
	pinMode(8,OUTPUT);
	Serial.setTimeout(-1);
	Serial.begin(9600);
	digitalWrite(8,HIGH);
	BindFunctionToPort([]() {
		return true;
	},
	                   UID::PortA_IsReady);
	BindFunctionToPort([]() {
		return static_cast<uint8_t>(sizeof(void const*));
	},
	                   UID::PortA_PointerSize);
#ifdef ARDUINO_ARCH_AVR
	BindFunctionToPort(std::ArduinoUrng::seed,
	                   UID::PortA_RandomSeed);
#endif
	BindFunctionToPort([]() {
		Process* P = new Process;
		ExistingProcesses.insert(P);
		return P;
	},
	                   UID::PortA_CreateProcess);
	BindFunctionToPort([](Process* P) {
		if (ExistingProcesses.erase(P)) {
			delete P;
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_DeleteProcess);
	BindFunctionToPort([](Process* P, UID ModuleID, uint16_t Times) -> ModuleStartReturn {
		if (ExistingProcesses.contains(P)) {
			auto const Iterator = SessionMap.find(ModuleID);
			if (Iterator == SessionMap.end())
				return { UID::Exception_InvalidModule };
			P->TrialsDone.clear();
			uint16_t NumTrials;
			if (Iterator->second(P, Times, NumTrials))
				SerialStream.AsyncInvoke(static_cast<uint8_t>(UID::PortC_ProcessFinished), P);
			return { UID::Exception_Success, NumTrials };
		}
		return { UID::Exception_InvalidProcess };
	},
	                   UID::PortA_StartModule);
	SerialListen([](uint8_t MessageSize) {
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
		std::unordered_map<UID, uint16_t>& TrialsDone = Header.P->TrialsDone;
		for (uint8_t i = 0; i < MessageSize; ++i) {
			UID const TrialID = SerialStream.Read<UID>();
			TrialsDone[TrialID] = SerialStream.Read<uint16_t>();
		}
		uint16_t NumTrials;
		bool const ProcessFinished = Iterator->second(Header.P, 1, NumTrials);
		SerialStream.Send(UID::Exception_Success, Header.RemotePort);
		if (ProcessFinished)
			SerialStream.AsyncInvoke(static_cast<uint8_t>(UID::PortC_ProcessFinished), Header.P);
	},
	             UID::PortA_RestoreModule);
	BindFunctionToPort([](Process* P) {
		if (ExistingProcesses.contains(P)) {
			P->Pause();
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_PauseProcess);
	BindFunctionToPort([](Process* P) {
		if (ExistingProcesses.contains(P)) {
			P->Continue();
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_ContinueProcess);
	BindFunctionToPort([](Process* P) {
		if (ExistingProcesses.contains(P)) {
			P->Abort();
			return UID::Exception_Success;
		}
		return UID::Exception_InvalidProcess;
	},
	                   UID::PortA_AbortProcess);
	SerialListen([](uint8_t MessageSize) {
		GbecHeader Header;
		if (CommonListenersHeader(MessageSize, Header))
			return;
		std::string const Info = Header.P->GetInfo();
		SerialStream.Send(Info.data(), Info.size(), Header.RemotePort);
	},
	             UID::PortA_GetInformation);
	SerialListen([](uint8_t MessageSize) {
		if (MessageSize < sizeof(uint8_t))
			return;
		Async_stream_IO::SendSession const Session{ sizeof(Process*) * ExistingProcesses.size(), SerialStream.Read<uint8_t>(), Serial };
		for (Process* const P : ExistingProcesses)
			Session << P;
	},
	             UID::PortA_AllProcesses);
}
void loop() {
	PinListener::ClearPending();
	SerialStream.ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>