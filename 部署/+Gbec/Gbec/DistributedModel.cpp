#pragma once
#include "UID.hpp"
#include "PinListener.hpp"
#include "Async_stream_IO.hpp"
#include <TimersOneForAll_Declare.hpp>
#include <map>
#include <random>
#include <sstream>
struct Process;
struct Module
{
	Process &Container;
	Module(Process &Container) : Container(Container) {}
	// 返回是否需要等待回调。回调函数只能在Start时提供，不能在构造时提供，因为存在复杂的虚继承关系，只能默认构造。
	virtual bool Start(std::move_only_function<void() const> const &FinishCallback) {}
	virtual void WriteInfo(std::ostringstream &InfoStream) const = 0;
	// 放弃该步骤。未在执行的步骤放弃也不会出错。
	virtual void Abort() {}
	// 重新开始当前执行中的步骤，不改变下一步。不应试图重启当前未在执行中的步骤。
	virtual void Restart() {}
	virtual ~Module() = default;
};
inline static void InfoWrite(std::ostringstream &InfoStream, UID InfoValue)
{
	InfoStream.put(static_cast<char>(InfoValue));
}
inline static void InfoWrite(std::ostringstream &InfoStream, uint8_t InfoValue)
{
	InfoStream.put(static_cast<char>(InfoValue));
}
inline static void InfoWrite(std::ostringstream &InfoStream, uint16_t InfoValue)
{
	InfoStream.write(reinterpret_cast<const char *>(&InfoValue), sizeof(InfoValue));
}
inline static void InfoWrite(std::ostringstream &InfoStream, UID const *InfoValue)
{
	InfoStream.write(reinterpret_cast<const char *>(&InfoValue), sizeof(InfoValue));
}
template <typename... T>
inline static void InfoWrite(std::ostringstream &InfoStream, T... InfoValues)
{
	int _[] = {(InfoWrite(InfoStream, InfoValues), 0)...};
}
extern Async_stream_IO::AsyncStream SerialStream;
struct Process
{
	void Pause() const
	{
		for (PinListener const *H : ActiveInterrupts)
			H->Pause();
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
			T->Pause();
	}
	void Continue() const
	{
		for (PinListener const *H : ActiveInterrupts)
			H->Continue();
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
			T->Continue();
	}
	void ModuleAbort()
	{
		_Abort();
		ActiveInterrupts.clear();
		ActiveTimers.clear();
		TrialsDone.clear();
	}
	virtual ~Process()
	{
		_Abort();
	}
	PinListener *RegisterInterrupt(uint8_t Pin, std::move_only_function<void() const> &&Callback)
	{
		PinListener *Listener = new PinListener(Pin, std::move(Callback));
		ActiveInterrupts.insert(Listener);
		return Listener;
	}
	void UnregisterInterrupt(PinListener *Handle)
	{
		delete Handle;
		ActiveInterrupts.erase(Handle);
	}
	Timers_one_for_all::TimerClass *AllocateTimer()
	{
		Timers_one_for_all::TimerClass *Timer = Timers_one_for_all::AllocateTimer();
		ActiveTimers.insert(Timer);
		return Timer;
	}
	// 此操作不负责停止计时器，仅使其可以被再分配
	void UnregisterTimer(Timers_one_for_all::TimerClass *Timer)
	{
		ActiveTimers.erase(Timer);
		Timer->Allocatable = true;
	}
	template <typename ModuleType>
	ModuleType *LoadModule()
	{
		auto Iter = Modules.find(&ModuleType::ID);
		if (Iter == Modules.end())
		{
			std::unique_ptr<Module> NewModule = std::make_unique<ModuleType>(*this);
			ModuleType *ModulePtr = NewModule.get();
			Modules[&ModuleType::ID] = std::move(NewModule);
			return ModulePtr;
		}
		return reinterpret_cast<ModuleType *>(Iter->second.get());
	}
	template <typename Entry>
	bool Start(std::move_only_function<void() const> const &FinishCallback)
	{
		StartModule = &Entry::ID;
		Entry *ModulePtr = LoadModule<Entry>();
		if (ModulePtr->Start(FinishCallback))
			return true;
		return false;
	}
	std::string GetInfo() const
	{
		std::ostringstream InfoStream;
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_StartModule, UID::Type_Pointer, StartModule, UID::Field_Modules, UID::Type_Map, Modules.size(), UID::Type_Pointer);
		for (auto const &[IDPtr, ModulePtr] : Modules)
		{
			InfoWrite(InfoStream, IDPtr);
			ModulePtr->WriteInfo(InfoStream);
		}
		return InfoStream.str();
	}
	std::unordered_map<UID, uint16_t> TrialsDone;
	uint8_t MessagePort = -1;

protected:
	std::set<PinListener *> ActiveInterrupts;
	std::set<Timers_one_for_all::TimerClass *> ActiveTimers;
	std::map<UID const *, std::unique_ptr<Module>> Modules;
	UID const *StartModule;
	// 中断不安全
	void _Abort()
	{
		for (PinListener *H : ActiveInterrupts)
			delete H;
		for (Timers_one_for_all::TimerClass *T : ActiveTimers)
		{
			T->Stop();
			T->Allocatable = true; // 使其可以被重新分配
		}
	}
};

static std::move_only_function<void() const> EmptyCallback = []() {};
template <typename Content, uint16_t Times>
struct Repeat : Module
{
	Repeat(Process &Container) : Module(Container) {}
	void Abort() override
	{
		ContentPtr->Abort();
	}
	void Restart() override
	{
		ContentPtr->Abort();
		for (RemainingRepeats = Times; RemainingRepeats > 0; --RemainingRepeats)
			if (ContentPtr->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		Abort();
		FinishCallback = &FC;
		for (RemainingRepeats = Times; RemainingRepeats > 0; --RemainingRepeats)
			if (ContentPtr->Start(NextBlock))
				return true;
		return false;
	}

protected:
	Content *const ContentPtr = Container.LoadModule<Content>();
	uint16_t RemainingRepeats;
	std::move_only_function<void() const> const *FinishCallback = &EmptyCallback;
	std::move_only_function<void() const> const NextBlock = [this]()
	{
		while (--RemainingRepeats)
			if (ContentPtr->Start(NextBlock))
				return;
		(*FinishCallback)();
	};
};
template <typename... SubModules>
struct Sequential : Module
{
	static constexpr UID ID = UID::Module_Sequential;
	Sequential(Process &Container) : Module(Container) {}
	void Abort() override
	{
		if (CurrentModule < std::end(SubPointers))
			(*CurrentModule)->Abort();
	}
	void Restart() override
	{
		Abort();
		for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		Abort();
		FinishCallback = &FC;
		for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return true;
		return false;
	}
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Modules, UID::Type_Array, sizeof...(SubModules), UID::Type_Pointer, &SubModules::ID...);
	}

protected:
	Module *const SubPointers[] = {Container.LoadModule<SubModules>()...};
	Module *const *CurrentModule = std::end(SubPointers);
	std::move_only_function<void() const> const *FinishCallback = &EmptyCallback;
	std::move_only_function<void() const> const NextBlock = [this]()
	{
		while (++CurrentModule < std::end(SubPointers))
			if ((*CurrentModule)->Start(NextBlock))
				return;
		(*FinishCallback)();
	};
};

struct IRandom
{
	virtual void Randomize() = 0;
};
template <typename... SubModules>
struct RandomSequential : Module, IRandom
{
	static constexpr
#ifdef ARDUINO_ARCH_AVR
		std::ArduinoUrng
#endif
#ifdef ARDUINO_ARCH_SAM
			std::TrueUrng
#endif
				Urng;
	template <uint16_t Repeats>
	struct WithRepeat : Module, IRandom
	{
		static constexpr UID ID = UID::Module_RandomSequential;
		void Randomize() override
		{
			std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
		}
		WithRepeat(Process &Container) : Module(Container)
		{
			auto _[] = {CurrentModule = std::fill_n(CurrentModule, Repeats, Container.LoadModule<SubModules>())...};
			Randomize();
		}
		void Abort() override
		{
			if (CurrentModule < std::end(SubPointers))
				(*CurrentModule)->Abort();
		}
		void Restart() override
		{
			Abort();
			for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
				if ((*CurrentModule)->Start(FinishCallback))
					return;
		}
		bool Start(std::move_only_function<void() const> const &FC) override
		{
			Abort();
			FinishCallback = &FC;
			for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
				if ((*CurrentModule)->Start(NextBlock))
					return true;
			return false;
		}
		void WriteInfo(std::ostringstream &InfoStream) const override
		{
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Modules, UID::Type_Table, sizeof...(SubModules), 2, UID::Column_Module, UID::Type_Pointer, &SubModules::ID..., UID::Column_Repeats, UID::Type_UInt16, Repeats...);
		}

	protected:
		template <uint16_t First, uint16_t... Rest>
		struct Sum
		{
			static constexpr uint16_t value = First + Sum<Rest...>::value;
		};
		template <uint16_t Only>
		struct Sum<Only>
		{
			static constexpr uint16_t value = Only;
		};
		Module *SubPointers[Sum<Repeats...>::value];
		Module **CurrentModule = std::begin(SubPointers);
		std::move_only_function<void() const> const *FinishCallback = &EmptyCallback;
		std::move_only_function<void() const> const NextBlock = [this]()
		{
			while (++CurrentModule < std::end(SubPointers))
				if ((*CurrentModule)->Start(NextBlock))
					return;
			(*FinishCallback)();
		};
	};
	static constexpr UID ID = UID::Module_RandomSequential;
	void Randomize() override
	{
		std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
	}
	RandomSequential(Process &Container) : Module(Container)
	{
		Randomize();
	}
	void Abort() override
	{
		if (CurrentModule < std::end(SubPointers))
			(*CurrentModule)->Abort();
	}
	void Restart() override
	{
		Abort();
		for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(FinishCallback))
				return;
	}
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		Abort();
		FinishCallback = &FC;
		for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return true;
		return false;
	}
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Modules, UID::Type_Array, sizeof...(SubModules), UID::Type_Pointer, &SubModules::ID...);
	}

protected:
	Module *SubPointers[] = {Container.LoadModule<SubModules>()...};
	Module *const *CurrentModule = std::end(SubPointers);
	std::move_only_function<void() const> const *FinishCallback = &EmptyCallback;
	std::move_only_function<void() const> const NextBlock = [this]()
	{
		while (++CurrentModule < std::end(SubPointers))
			if ((*CurrentModule)->Start(NextBlock))
				return;
		(*FinishCallback)();
	};
};
template <typename T>
struct _TypeID;
template <>
struct _TypeID<std::chrono::seconds>
{
	static constexpr UID value = UID::Type_Seconds;
};
template <>
struct _TypeID<std::chrono::milliseconds>
{
	static constexpr UID value = UID::Type_Milliseconds;
};
// 表示一个常数时间段。Unit必须是std::chrono::duration的一个实例。
template <typename Unit, uint16_t Value>
struct ConstantDuration;
// 表示一个随机时间段。Unit必须是std::chrono::duration的一个实例。该步骤维护一个随机变量，只有使用Randomize步骤才能改变这个变量，否则一直保持相同的值。如果有多个随机范围相同的随机变量需要独立控制随机性，可以指定不同的ID，否则视为同一个随机变量。
template <typename Unit, uint16_t Min, uint16_t Max, UID CustomID = UID::Duration_Random>
struct RandomDuration : Module, IRandom
{
	Unit Current;
	void Randomize() override
	{
		constexpr double DoubleMin = Min;
		Current = Unit{pow(static_cast<double>(Max) / DoubleMin, static_cast<double>(random(__LONG_MAX__)) / (__LONG_MAX__ - 1)) * DoubleMin};
	}
	RandomDuration(Process &Container) : Module(Container)
	{
		Randomize();
	}
	static constexpr UID ID = CustomID;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Range, UID::Type_Array, 2, _TypeID<Unit>, Min, Max);
	}
};
struct InfiniteDuration;
struct _Delay : Module
{
	_Delay(Process &Container) : Module(Container) {}
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		if (Container.TrialsDone.size())
			return false;
		FinishCallback = &FC;
		Restart();
		return true;
	}
	void Abort() override
	{
		if (Timer)
		{
			Timer->Stop();
			Container.UnregisterTimer(Timer);
			Timer = nullptr;
		}
	}
	static constexpr UID ID = UID::Module_Delay;

protected:
	Timers_one_for_all::TimerClass *Timer = nullptr;
	std::move_only_function<void() const> const *FinishCallback = &EmptyCallback;
};
template <typename RandomDuration>
struct Delay : _Delay
{
	Delay(Process &Container) : _Delay(Container) {}
	void Restart() override
	{
		if (!Timer)
			Timer = Container.AllocateTimer();
		Timer->DoAfter(DurationPtr->Current, [this]()
					   {
			Container.UnregisterTimer(Timer);
			Timer = nullptr;
			(*FinishCallback)(); });
	}
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, UID::Type_Pointer, &RandomDuration::ID);
	}

protected:
	RandomDuration const *const DurationPtr = Container.LoadModule<RandomDuration>();
};
template <typename Unit, uint16_t Value>
struct Delay<ConstantDuration<Unit, Value>> : _Delay
{
	Delay(Process &Container) : _Delay(Container) {}
	void Restart() override
	{
		if (!Timer)
			Timer = Container.AllocateTimer();
		Timer->DoAfter(Unit{Value}, [this]()
					   {
				Container.UnregisterTimer(Timer);
				Timer = nullptr;
			(*FinishCallback)(); });
	}
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, _TypeID<Unit>::value, Value);
	}
};
template <>
struct Delay<InfiniteDuration> : Module
{
	Delay(Process &Container) : Module(Container) {}
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		if (Container.TrialsDone.size())
			return false;
		return true;
	}
	static constexpr UID ID = UID::Module_Delay;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, UID::Type_UID, UID::Duration_Infinite);
	}
};
struct InstantaneousModule : Module
{
	InstantaneousModule(Process &Container) : Module(Container) {}
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		if (Container.TrialsDone.empty())
			Restart();
		return false;
	}
};
template <typename Target>
struct ModuleAbort : InstantaneousModule
{
	ModuleAbort(Process &Container) : InstantaneousModule(Container) {}
	void Restart() override
	{
		TargetPtr->Abort();
	}
	static constexpr UID ID = UID::Module_Abort;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID);
	}

protected:
	Module *const TargetPtr = Container.LoadModule<Target>();
};
template <typename Target>
struct ModuleRestart : InstantaneousModule
{
	ModuleRestart(Process &Container) : InstantaneousModule(Container) {}
	void Restart() override
	{
		TargetPtr->Restart();
	}
	static constexpr UID ID = UID::Module_Restart;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID);
	}

protected:
	Module *const TargetPtr = Container.LoadModule<Target>();
};
template <typename Target>
struct ModuleRandomize : InstantaneousModule
{
	ModuleRandomize(Process &Container) : InstantaneousModule(Container) {}
	void Restart() override
	{
		TargetPtr->Randomize();
	}
	static constexpr UID ID = UID::Module_Randomize;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID);
	}

protected:
	IRandom *const TargetPtr = reinterpret_cast<IRandom *>(Container.LoadModule<Target>());
};
template <uint8_t Pin, bool HighOrLow>
struct DigitalWrite : InstantaneousModule
{
	DigitalWrite(Process &Container) : InstantaneousModule(Container)
	{
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	void Restart() override
	{
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
	}
	static constexpr UID ID = UID::Module_DigitalWrite;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, ID, UID::Field_Pin, UID::Type_UInt8, Pin, UID::Field_HighOrLow, UID::Type_Bool, HighOrLow);
	}
};
// 此模块可以用ModuleAbort停止监视
template <uint8_t Pin, typename Monitor>
struct MonitorPin : InstantaneousModule
{
	MonitorPin(Process &Container) : InstantaneousModule(Container)
	{
		Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
	}
	PinListener *Listener = nullptr;
	void Abort() override
	{
		if (Listener)
		{
			Container.UnregisterInterrupt(Listener);
			Listener = nullptr;
		}
	}
	void Restart() override
	{
		Abort();
		Listener = Container.RegisterInterrupt(Pin, [MonitorPtr]()
											   { MonitorPtr->Start(EmptyCallback); });
	}
	static constexpr UID ID = UID::Module_MonitorPin;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, ID, UID::Field_Pin, UID::Type_UInt8, Pin, UID::Field_Monitor, UID::Type_Pointer, &Monitor::ID);
	}

protected:
	Monitor *const MonitorPtr = Container.LoadModule<Monitor>();
};
template <UID Message>
struct SerialMessage : InstantaneousModule
{
	SerialMessage(Process &Container) : InstantaneousModule(Container) {}
	void Restart() override
	{
		SerialStream.Send(Message, Container.MessagePort);
	}
	static constexpr UID ID = UID::Module_SerialMessage;
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Message, UID::Type_UID, Message);
	}
};
// 回合是断线重连后自动恢复的最小单元
template <UID TrialID, typename Content>
struct Trial : Module
{
	Trial(Process &Container) : Module(Container) {}
	void Abort() override
	{
		ContentPtr->Abort();
	}
	void Restart() override
	{
		ContentPtr->Restart();
	}
	static constexpr UID ID = TrialID;
	bool Start(std::move_only_function<void() const> const &FC) override
	{
		auto It = Container.TrialsDone.find(TrialID);
		if (It != Container.TrialsDone.end())
			if (It->second)
			{
				if (!--It->second)
					Container.TrialsDone.erase(It);
				return false;
			}
			else
				Container.TrialsDone.erase(It);
		Async_stream_IO::SendSession S{2 * sizeof(UID), Container.MessagePort, Serial};
		S << UID::Signal_TrialStart << TrialID;
		return ContentPtr->Start(FC);
	}
	void WriteInfo(std::ostringstream &InfoStream) const override
	{
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Content, UID::Type_Pointer, &Content::ID);
	}

protected:
	Content *const ContentPtr = Container.LoadModule<Content>();
};
template <typename Target, typename Cleaner>
struct CleanWhenAbort : Module
{
	CleanWhenAbort(Process &Container) : Module(Container) {}
	void Abort()ove

protected:
	Target *const TargetPtr = Container.LoadModule<Target>();
	Cleaner *const CleanerPtr = Container.LoadModule<Cleaner>();
};