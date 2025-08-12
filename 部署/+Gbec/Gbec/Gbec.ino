#include "Predefined.hpp"
// SAM编译器bug，此定义必须放前面否则找不到
#pragma pack(push, 1)
struct GbecHeader
{
	uint8_t RemotePort;
	Process* P;
};
#pragma pack(pop)
std::queue<std::move_only_function<void() const> const*> PinListener::PendingCallbacks;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> PinListener::Listening;
std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> PinListener::Resting;
std::move_only_function<void() const> const Module::_EmptyCallback = []() {};
Async_stream_IO::AsyncStream SerialStream{ Serial };
extern std::unordered_map<UID, void (*)(Process&, std::move_only_function<void() const> const&)> SessionMap;
static std::set<Process*> ExistingProcesses;

template <typename T>
inline void BindFunctionToPort(T&& Function, UID Port)
{
	SerialStream.BindFunctionToPort(std::forward<T>(Function), static_cast<uint8_t>(Port));
}
template <typename T>
inline void SerialListen(T&& Callback, UID Port)
{
	SerialStream.Listen(std::forward<T>(Callback), static_cast<uint8_t>(Port));
}
bool CommonListenersHeader(uint8_t& MessageSize, GbecHeader& Header)
{
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

void setup()
{
	Serial.setTimeout(-1);
	Serial.begin(9600);
	BindFunctionToPort([]()
		{ return true; },
		UID::PortA_IsReady);
	BindFunctionToPort([]()
		{ return static_cast<uint8_t>(sizeof(void const*)); },
		UID::PortA_PointerSize);
#ifdef ARDUINO_ARCH_AVR
	BindFunctionToPort(std::ArduinoUrng::seed,
		UID::PortA_RandomSeed);
#endif
	BindFunctionToPort([](uint8_t MessagePort)
		{
			Process* P = new Process(MessagePort);
			ExistingProcesses.insert(P);
			return P; },
		UID::PortA_CreateProcess);
	BindFunctionToPort([](Process* P)
		{
			if (ExistingProcesses.erase(P)) {
				delete P;
				return UID::Exception_Success;
			}
			return UID::Exception_InvalidProcess; },
		UID::PortA_DeleteProcess);
	BindFunctionToPort([](Process* P, UID ModuleID)
		{ 
			if (ExistingProcesses.contains(P)) {
				auto const Iterator=SessionMap.find(ModuleID);
				if(Iterator==SessionMap.end())
					return UID::Exception_InvalidModule;
					Iterator->second(P,[]())
			}
			return UID::Exception_InvalidProcess;
		 },
		UID::PortA_StartModule);
	BindFunctionToPort([](Process* P, uint8_t Times)
		{ return Process::Existing.contains(P) ? P->Repeat(Times) : UID::Exception_InvalidProcess; },
		UID::PortA_RepeatProcess);
	SerialListen([](uint8_t MessageSize)
		{
			GbecHeader Header;
			if (CommonListenersHeader(MessageSize, Header))
				return;
			MessageSize /= (sizeof(UID) + sizeof(uint16_t));
			std::unordered_map<UID, uint16_t> TrialsDone;
			TrialsDone.reserve(MessageSize);
			for (uint8_t i = 0; i < MessageSize; ++i) {
				UID const TrialID = SerialStream.Read<UID>();
				TrialsDone[TrialID] = SerialStream.Read<uint16_t>();
			}
			SerialStream.Send(Header.P->Restore(std::move(TrialsDone)), Header.RemotePort); },
		UID::PortA_RestoreProcess);
	BindFunctionToPort([](Process* P)
		{ return Process::Existing.contains(P) ? P->Pause() : UID::Exception_InvalidProcess; },
		UID::PortA_PauseProcess);
	BindFunctionToPort([](Process* P)
		{ return Process::Existing.contains(P) ? P->Continue() : UID::Exception_InvalidProcess; },
		UID::PortA_ContinueProcess);
	BindFunctionToPort([](Process* P)
		{ return Process::Existing.contains(P) ? P->Abort() : UID::Exception_InvalidProcess; },
		UID::PortA_AbortProcess);
	SerialListen([](uint8_t MessageSize)
		{
			GbecHeader Header;
			if (CommonListenersHeader(MessageSize, Header))
				return;
			std::ostringstream OutStream;
			Header.P->WriteInfo(OutStream);
			std::string const Info = OutStream.str();
			SerialStream.Send(Info.data(), Info.size(), Header.RemotePort); },
		UID::PortA_GetInformation);
	SerialListen([](uint8_t MessageSize)
		{
			if (MessageSize < sizeof(uint8_t))
				return;
			Async_stream_IO::SendSession const Session{ sizeof(Process*) * Process::Existing.size(), SerialStream.Read<uint8_t>(), Serial };
			for (Process* const P : Process::Existing)
				Session << P; },
		UID::PortA_AllProcesses);
}
std::set<std::move_only_function<void() const> const*> _PendingInterrupts;
void loop()
{
	noInterrupts(); // 不交换到本地，直接禁止中断执行，以控制中断频率
	for (auto M : _PendingInterrupts)
		M->operator()();
	_PendingInterrupts.clear();
	interrupts();
	SerialStream.ExecuteTransactionsInQueue();
}
#include <TimersOneForAll_Define.hpp>