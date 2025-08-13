#pragma once
#include "UID.hpp"
#include "Async_stream_IO.hpp"
#include "Timers_one_for_all.hpp"
#include <Quick_digital_IO_interrupt.hpp>
#include <map>
#include <random>
#include <sstream>
#include <functional>
#include <queue>
#include <set>
#include <iterator>
struct PinListener {
	uint8_t const Pin;
	std::move_only_function<void() const> const Callback;
	// 中断不安全
	void Pause() const {
		std::set<std::move_only_function<void() const> const*>* CallbackSet = &Listening[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		CallbackSet = &Resting[Pin];
		CallbackSet->erase(&Callback);
		if (CallbackSet->empty())
			Resting.erase(Pin);
	}
	// 中断不安全
	void Continue() const {
		std::set<std::move_only_function<void() const> const*>& CallbackSet = Listening[Pin];
		if (CallbackSet.empty())
			Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Pin, PinInterrupt{ Pin });
		CallbackSet.insert(&Callback);
	}
	// 中断不安全
	PinListener(uint8_t Pin, std::move_only_function<void() const>&& Callback)
	  : Pin(Pin), Callback(std::move(Callback)) {
		Continue();
	}
	// 中断不安全
	~PinListener() {
		Pause();
	}
	// 中断安全
	static void ClearPending() {
		static std::queue<std::move_only_function<void() const> const*> LocalSwap;
		{
			Quick_digital_IO_interrupt::InterruptGuard const _;
			std::swap(LocalSwap, PendingCallbacks);
			for (auto& Iterator : Resting) {
				std::set<std::move_only_function<void() const> const*>& ListeningSet = Listening[Iterator.first];
				ListeningSet.merge(Iterator.second);
				if (ListeningSet.size())
					Quick_digital_IO_interrupt::AttachInterrupt<RISING>(Iterator.first, PinInterrupt{ Iterator.first });
				Iterator.second.clear();
			}
		}
		while (LocalSwap.size()) {
			(*LocalSwap.front())();
			LocalSwap.pop();
		}
	}

protected:
	static std::queue<std::move_only_function<void() const> const*> PendingCallbacks;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> Listening;
	static std::unordered_map<uint8_t, std::set<std::move_only_function<void() const> const*>> Resting;
	struct PinInterrupt {
		uint8_t const Pin;
		void operator()() const {
			std::set<std::move_only_function<void() const> const*>& Listenings = Listening[Pin];
			for (auto H : Listenings)
				PendingCallbacks.push(H);
			Resting[Pin].merge(Listenings);
			Listenings.clear();
			Quick_digital_IO_interrupt::DetachInterrupt(Pin);
		}
	};
};
struct Process;
struct Module {
	Process& Container;
	Module(Process& Container)
	  : Container(Container) {
	}
	// 返回是否需要等待回调。回调函数只能在Start时提供，不能在构造时提供，因为存在复杂的虚继承关系，只能默认构造。
	virtual bool Start(std::move_only_function<void() const> const& FinishCallback) {}
	virtual void WriteInfo(std::ostringstream& InfoStream) const = 0;
	// 放弃该步骤。未在执行的步骤放弃也不会出错。
	virtual void Abort() {}
	// 重新开始当前执行中的步骤，不改变下一步。不应试图重启当前未在执行中的步骤。
	virtual void Restart() {}
	virtual ~Module() = default;
	static std::move_only_function<void() const> const _EmptyCallback;
	struct _EmptyStart {
		Module* const ContentModule;
		void operator()() const {
			ContentModule->Start(_EmptyCallback);
		}
	};
};
inline static void InfoWrite(std::ostringstream& InfoStream, UID InfoValue) {
	InfoStream.put(static_cast<char>(InfoValue));
}
inline static void InfoWrite(std::ostringstream& InfoStream, uint8_t InfoValue) {
	InfoStream.put(static_cast<char>(InfoValue));
}
inline static void InfoWrite(std::ostringstream& InfoStream, uint16_t InfoValue) {
	InfoStream.write(reinterpret_cast<const char*>(&InfoValue), sizeof(InfoValue));
}
inline static void InfoWrite(std::ostringstream& InfoStream, UID const* InfoValue) {
	InfoStream.write(reinterpret_cast<const char*>(&InfoValue), sizeof(InfoValue));
}
template<typename... T>
inline static void InfoWrite(std::ostringstream& InfoStream, T... InfoValues) {
	int _[] = { (InfoWrite(InfoStream, InfoValues), 0)... };
}
extern Async_stream_IO::AsyncStream SerialStream;
struct Process {
	void Pause() const {
		for (PinListener const* H : ActiveInterrupts)
			H->Pause();
		for (Timers_one_for_all::TimerClass* T : ActiveTimers)
			T->Pause();
	}
	void Continue() const {
		for (PinListener const* H : ActiveInterrupts)
			H->Continue();
		for (Timers_one_for_all::TimerClass* T : ActiveTimers)
			T->Continue();
	}
	void Abort() {
		_Abort();
		ActiveInterrupts.clear();
		ActiveTimers.clear();
		TrialsDone.clear();
		ExtraCleaners.clear();
	}
	virtual ~Process() {
		_Abort();
	}
	PinListener* RegisterInterrupt(uint8_t Pin, std::move_only_function<void() const>&& Callback) {
		PinListener* Listener = new PinListener(Pin, std::move(Callback));
		ActiveInterrupts.insert(Listener);
		return Listener;
	}
	void UnregisterInterrupt(PinListener* Handle) {
		delete Handle;
		ActiveInterrupts.erase(Handle);
	}
	Timers_one_for_all::TimerClass* AllocateTimer() {
		Timers_one_for_all::TimerClass* Timer = Timers_one_for_all::AllocateTimer();
		ActiveTimers.insert(Timer);
		return Timer;
	}
	// 此操作不负责停止计时器，仅使其可以被再分配
	void UnregisterTimer(Timers_one_for_all::TimerClass* Timer) {
		ActiveTimers.erase(Timer);
		Timer->Allocatable = true;
	}
	template<typename ModuleType>
	ModuleType* LoadModule() {
		auto Iter = Modules.find(&ModuleType::ID);
		if (Iter == Modules.end()) {
			std::unique_ptr<Module> NewModule = std::make_unique<ModuleType>(*this);
			ModuleType* ModulePtr = NewModule.get();
			Modules[&ModuleType::ID] = std::move(NewModule);
			return ModulePtr;
		}
		return reinterpret_cast<ModuleType*>(Iter->second.get());
	}
	template<typename Entry>
	bool Start(uint16_t Times) {
		StartPointer = &Entry::ID;
		StartModule = LoadModule<Entry>();
		for (TimesLeft = Times; TimesLeft > 0; --TimesLeft)
			if (StartModule->Start(FinishCallback))
				return true;
		return false;
	}
	std::string GetInfo() const {
		std::ostringstream InfoStream;
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_StartModule, UID::Type_Pointer, StartPointer, UID::Field_Modules, UID::Type_Map, Modules.size(), UID::Type_Pointer);
		for (auto const& Iterator : Modules) {
			InfoWrite(InfoStream, Iterator.first);
			Iterator.second->WriteInfo(InfoStream);
		}
		return InfoStream.str();
	}
	std::unordered_map<UID, uint16_t> TrialsDone;
	std::set<std::move_only_function<void() const> const*> ExtraCleaners;

protected:
	std::set<PinListener*> ActiveInterrupts;
	std::set<Timers_one_for_all::TimerClass*> ActiveTimers;
	std::map<UID const*, std::unique_ptr<Module>> Modules;
	UID const* StartPointer;
	Module* StartModule;
	uint16_t TimesLeft;
	std::move_only_function<void() const> const FinishCallback{
		[this]() {
		  while (--TimesLeft)
			  if (StartModule->Start(FinishCallback)) return;
		  SerialStream.AsyncInvoke(static_cast<uint8_t>(UID::PortC_ProcessFinished), this);
		}
	};

	// 中断不安全
	void _Abort() {
		for (PinListener* H : ActiveInterrupts)
			delete H;
		for (Timers_one_for_all::TimerClass* T : ActiveTimers) {
			T->Stop();
			T->Allocatable = true;  // 使其可以被重新分配
		}
		for (std::move_only_function<void() const> const* Cleaner : ExtraCleaners)
			(*Cleaner)();
	}
};

template<typename Content, uint16_t Times>
struct Repeat : Module {
	Repeat(Process& Container)
	  : Module(Container) {
	}
	void Abort() override {
		ContentPtr->Abort();
	}
	void Restart() override {
		ContentPtr->Abort();
		for (RemainingRepeats = Times; RemainingRepeats > 0; --RemainingRepeats)
			if (ContentPtr->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		Abort();
		for (RemainingRepeats = Times; RemainingRepeats > 0; --RemainingRepeats)
			if (ContentPtr->Start(NextBlock)) {
				FinishCallback = &FC;
				return true;
			}
		return false;
	}

protected:
	Content* const ContentPtr = Module::Container.LoadModule<Content>();
	uint16_t RemainingRepeats;
	std::move_only_function<void() const> const* FinishCallback = &_EmptyCallback;
	std::move_only_function<void() const> const NextBlock = [this]() {
		while (--RemainingRepeats)
			if (ContentPtr->Start(NextBlock))
				return;
		(*FinishCallback)();
	};
};
template<typename... SubModules>
struct Sequential : Module {
	static constexpr UID ID = UID::Module_Sequential;
	Sequential(Process& Container)
	  : Module(Container) {
	}
	void Abort() override {
		if (CurrentModule < std::cend(SubPointers))
			(*CurrentModule)->Abort();
	}
	void Restart() override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock)) {
				FinishCallback = &FC;
				return true;
			}
		return false;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Modules, UID::Type_Array, sizeof...(SubModules), UID::Type_Pointer, &SubModules::ID...);
	}

protected:
	Module* const SubPointers[sizeof...(SubModules)] = { Module::Container.LoadModule<SubModules>()... };
	Module* const* CurrentModule = std::cend(SubPointers);
	std::move_only_function<void() const> const* FinishCallback = &_EmptyCallback;

	// 必须预设NextBlock，这样即使没有Start直接Restart也能执行完整个模块
	std::move_only_function<void() const> const NextBlock = [this]() {
		while (++CurrentModule < std::cend(SubPointers))
			if ((*CurrentModule)->Start(NextBlock))
				return;
		(*FinishCallback)();
	};
};

struct IRandom {
	virtual void Randomize() = 0;
};
template<typename... SubModules>
struct RandomSequential : Module, IRandom {
	static constexpr
#ifdef ARDUINO_ARCH_AVR
	  std::ArduinoUrng
#endif
#ifdef ARDUINO_ARCH_SAM
	    std::TrueUrng
#endif
	      Urng{};
	template<uint16_t... Repeats>
	struct WithRepeat : Module, IRandom {
		static constexpr UID ID = UID::Module_RandomSequential;
		void Randomize() override {
			std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
		}
		WithRepeat(Process& Container)
		  : Module(Container) {
			auto _[] = { CurrentModule = std::fill_n(CurrentModule, Repeats, Module::Container.LoadModule<SubModules>())... };
			Randomize();
		}
		void Abort() override {
			if (CurrentModule < std::end(SubPointers))
				(*CurrentModule)->Abort();
		}
		void Restart() override {
			Abort();
			for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
				if ((*CurrentModule)->Start(NextBlock))
					return;
		}
		bool Start(std::move_only_function<void() const> const& FC) override {
			Abort();
			for (CurrentModule = std::begin(SubPointers); CurrentModule < std::end(SubPointers); ++CurrentModule)
				if ((*CurrentModule)->Start(NextBlock)) {
					FinishCallback = &FC;
					return true;
				}
			return false;
		}
		void WriteInfo(std::ostringstream& InfoStream) const override {
			InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Modules, UID::Type_Table, sizeof...(SubModules), 2, UID::Column_Module, UID::Type_Pointer, &SubModules::ID..., UID::Column_Repeats, UID::Type_UInt16, Repeats...);
		}

	protected:
		template<uint16_t First, uint16_t... Rest>
		struct Sum {
			static constexpr uint16_t value = First + Sum<Rest...>::value;
		};
		template<uint16_t Only>
		struct Sum<Only> {
			static constexpr uint16_t value = Only;
		};
		Module* SubPointers[Sum<Repeats...>::value];
		Module** CurrentModule = std::begin(SubPointers);
		std::move_only_function<void() const> const* FinishCallback = &_EmptyCallback;
		std::move_only_function<void() const> const NextBlock = [this]() {
			while (++CurrentModule < std::end(SubPointers))
				if ((*CurrentModule)->Start(NextBlock))
					return;
			(*FinishCallback)();
		};
	};
	static constexpr UID ID = UID::Module_RandomSequential;
	void Randomize() override {
		std::shuffle(std::begin(SubPointers), std::end(SubPointers), Urng);
	}
	RandomSequential(Process& Container)
	  : Module(Container) {
		Randomize();
	}
	void Abort() override {
		if (CurrentModule < std::cend(SubPointers))
			(*CurrentModule)->Abort();
	}
	void Restart() override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock))
				return;
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		Abort();
		for (CurrentModule = std::cbegin(SubPointers); CurrentModule < std::cend(SubPointers); ++CurrentModule)
			if ((*CurrentModule)->Start(NextBlock)) {
				FinishCallback = &FC;
				return true;
			}
		return false;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Modules, UID::Type_Array, sizeof...(SubModules), UID::Type_Pointer, &SubModules::ID...);
	}

protected:
	Module* SubPointers[sizeof...(SubModules)] = { Module::Container.LoadModule<SubModules>()... };  //SAM编译器不能自动推断数组长度，导致cend不可用
	Module* const* CurrentModule = std::cend(SubPointers);
	std::move_only_function<void() const> const* FinishCallback = &_EmptyCallback;
	std::move_only_function<void() const> const NextBlock = [this]() {
		while (++CurrentModule < std::cend(SubPointers))
			if ((*CurrentModule)->Start(NextBlock))
				return;
		(*FinishCallback)();
	};
};
template<typename T>
struct _TypeID;
template<>
struct _TypeID<std::chrono::seconds> {
	static constexpr UID value = UID::Type_Seconds;
};
template<>
struct _TypeID<std::chrono::milliseconds> {
	static constexpr UID value = UID::Type_Milliseconds;
};
// 表示一个常数时间段。Unit必须是std::chrono::duration的一个实例。
template<typename Unit, uint16_t Value>
struct ConstantDuration;
// 表示一个随机时间段。Unit必须是std::chrono::duration的一个实例。该步骤维护一个随机变量，只有使用Randomize步骤才能改变这个变量，否则一直保持相同的值。如果有多个随机范围相同的随机变量需要独立控制随机性，可以指定不同的ID，否则视为同一个随机变量。
template<typename Unit, uint16_t Min, uint16_t Max, UID CustomID = UID::Duration_Random>
struct RandomDuration : Module, IRandom {
	Unit Current;
	void Randomize() override {
		constexpr double DoubleMin = Min;
		Current = Unit{ pow(static_cast<double>(Max) / DoubleMin, static_cast<double>(random(__LONG_MAX__)) / (__LONG_MAX__ - 1)) * DoubleMin };
	}
	RandomDuration(Process& Container)
	  : Module(Container) {
		Randomize();
	}
	static constexpr UID ID = CustomID;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Range, UID::Type_Array, 2, _TypeID<Unit>::value, Min, Max);
	}
};
struct InfiniteDuration;
struct _TimedModule : Module {
	_TimedModule(Process& Container)
	  : Module(Container) {
	}
	void Abort() override {
		if (Timer) {
			Timer->Stop();
			UnregisterTimer();
		}
	}

protected:
	Timers_one_for_all::TimerClass* Timer = nullptr;
	// 不检查当前Timer是否有效
	void UnregisterTimer() {
		Module::Container.UnregisterTimer(Timer);
		Timer = nullptr;
	}
	struct _UnregisterTimer {
		_TimedModule* const ContentModule;
		void operator()() const {
			ContentModule->UnregisterTimer();
		}
	};
};
struct _Delay : _TimedModule {
	_Delay(Process& Container)
	  : _TimedModule(Container) {
	}
	void Restart() override {
		if (!Timer)
			Timer = Module::Container.AllocateTimer();
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;

		// 在冷静阶段，Restart会被高频执行，因此只能牺牲一下Start，确保Restart的效率
		FinishCallback = [this, &FC]() {
			UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	static constexpr UID ID = UID::Module_Delay;

protected:
	// 确保直接Restart也能正常释放计时器
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
};
template<typename RandomDuration>
struct Delay : _Delay {
	Delay(Process& Container)
	  : _Delay(Container) {
	}
	void Restart() override {
		_TimedModule::Restart();
		_TimedModule::Timer->DoAfter(DurationPtr->Current, FinishCallback);
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, UID::Type_Pointer, &RandomDuration::ID);
	}

protected:
	RandomDuration const* const DurationPtr = Module::Container.LoadModule<RandomDuration>();
};
template<typename Unit, uint16_t Value>
struct Delay<ConstantDuration<Unit, Value>> : _Delay {
	Delay(Process& Container)
	  : _Delay(Container) {
	}
	void Restart() override {
		_TimedModule::Restart();
		_TimedModule::Timer->DoAfter(Unit{ Value }, FinishCallback);
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, _TypeID<Unit>::value, Value);
	}
};
template<>
struct Delay<InfiniteDuration> : Module {
	Delay(Process& Container)
	  : Module(Container) {
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		return true;
	}
	static constexpr UID ID = UID::Module_Delay;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Duration, UID::Type_UID, UID::Duration_Infinite);
	}
};
template<typename Content>
struct _RepeatEvery : _TimedModule {
	_RepeatEvery(Process& Container)
	  : _TimedModule(Container) {
	}
	void Abort() override {
		ContentPtr->Abort();
		_TimedModule::Abort();
	}
	void Restart() override {
		if (Timer)
			ContentPtr->Abort();
		else
			Timer = Module::Container.AllocateTimer();
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		// 默认的无限重复，不需要FinishCallback。
		Restart();
		return true;
	}
	static constexpr UID ID = UID::Module_RepeatEvery;

protected:
	Content* const ContentPtr = Module::Container.LoadModule<Content>();
	std::move_only_function<void() const> const RepeatCallback = _EmptyStart{ ContentPtr };
};
template<typename Period, typename Content, typename Duration>
struct _RepeatEvery_Random_UntilDuration;
/*
每隔指定周期，执行一次内容模块。这个周期就是最终的绝对的周期长度，内容模块异步执行，不占用周期时间。第一次执行也需要等待周期时长之后。
此模块为固定周期的高频循环优化，比组合使用Repeat和Delay效率更高更方便。但是，一旦此模块开始，周期长度即固定。如果采用随机周期值，那个周期值将在模块开始时确定，后续再改变随机变量值也不会改变此模块的周期，除非重启。因此，不能使用此模块实现每个周期时长随机。要实现此效果，请组合使用Repeat和Delay模块。
*/
template<typename Period, typename Content>
struct RepeatEvery : _RepeatEvery<Content> {
	template<uint16_t Times>
	struct UntilTimes : RepeatEvery {
		UntilTimes(Process& Container)
		  : RepeatEvery(Container) {
		}
		void Restart() override {
			_RepeatEvery<Content>::Restart();
			_TimedModule::Timer->RepeatEvery(PeriodPtr->Current, _RepeatEvery<Content>::RepeatCallback, Times, FinishCallback);
		}
		bool Start(std::move_only_function<void() const> const& FC) override {
			if (!Times || Module::Container.TrialsDone.size())
				return false;
			FinishCallback = [this, &FC]() {
				_TimedModule::UnregisterTimer();
				FC();
			};
			Restart();
			return true;
		}
		void WriteInfo(std::ostringstream& InfoStream) const override {
			InfoWrite(InfoStream, UID::Type_Struct, 4, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Period, UID::Type_Pointer, &Period::ID, UID::Field_Times, UID::Type_UInt16, Times);
		}

	protected:
		std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	};
	template<typename Duration>
	using UntilDuration = _RepeatEvery_Random_UntilDuration<Period, Content, Duration>;
	RepeatEvery(Process& Container)
	  : _RepeatEvery<Content>(Container) {}
	void Restart() override {
		_RepeatEvery<Content>::Restart();
		_TimedModule::Timer->RepeatEvery(PeriodPtr->Current, _RepeatEvery<Content>::RepeatCallback);
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Duration, UID::Type_Pointer, &Period::ID);
	}

protected:
	Period const* const PeriodPtr = Module::Container.LoadModule<Period>();
};
template<typename Period, typename Content, typename Duration>
struct _RepeatEvery_Random_UntilDuration : RepeatEvery<Period, Content> {
	_RepeatEvery_Random_UntilDuration(Process& Container)
	  : RepeatEvery<Period, Content>(Container) {
	}
	void Restart() override {
		_RepeatEvery<Content>::Restart();
		_TimedModule::Timer->RepeatEvery(RepeatEvery<Period, Content>::PeriodPtr->Current, _RepeatEvery<Content>::RepeatCallback, DurationPtr->Current, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 4, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Period, UID::Type_Pointer, &Period::ID, UID::Field_Duration, UID::Type_Pointer, &Duration::ID);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	Duration const* const DurationPtr = Module::Container.LoadModule<Duration>();
};
template<typename Period, typename Content, typename Unit, uint16_t Value>
struct _RepeatEvery_Random_UntilDuration<Period, Content, ConstantDuration<Unit, Value>> : RepeatEvery<Period, Content> {
	_RepeatEvery_Random_UntilDuration(Process& Container)
	  : RepeatEvery<Period, Content>(Container) {
	}
	void Restart() override {
		_RepeatEvery<Content>::Restart();
		_TimedModule::Timer->RepeatEvery(RepeatEvery<Period, Content>::PeriodPtr->Current, _RepeatEvery<Content>::RepeatCallback, Unit{ Value }, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 4, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Period, UID::Type_Pointer, &Period::ID, UID::Field_Duration, _TypeID<Unit>::value, Value);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
};
template<typename Period, typename Content>
struct _RepeatEvery_Random_UntilDuration<Period, Content, InfiniteDuration> : RepeatEvery<Period, Content> {
	_RepeatEvery_Random_UntilDuration(Process& Container)
	  : RepeatEvery<Period, Content>(Container) {
	}
};
template<typename PeriodUnit, uint16_t PeriodValue, typename Content, typename Duration>
struct _RepeatEvery_Constant_UntilDuration : RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content> {
	_RepeatEvery_Constant_UntilDuration(Process& Container)
	  : RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content>(Container) {
	}
	void Restart() override {
		_RepeatEvery<Content>::Restart();
		_TimedModule::Timer->RepeatEvery(PeriodUnit{ PeriodValue }, _RepeatEvery<Content>::RepeatCallback, DurationPtr->Current, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 4, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Period, _TypeID<PeriodUnit>::value, PeriodValue, UID::Field_Duration, UID::Type_Pointer, &Duration::ID);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	Duration const* const DurationPtr = Module::Container.LoadModule<Duration>();
};
template<typename PeriodUnit, uint16_t PeriodValue, typename Content>
struct RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content> : _RepeatEvery<Content> {
	RepeatEvery(Process& Container)
	  : _RepeatEvery<Content>(Container) {}
	void Restart() override {
		_RepeatEvery<Content>::Restart();
		_TimedModule::Timer->RepeatEvery(PeriodUnit{ PeriodValue }, _RepeatEvery<Content>::RepeatCallback);
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Duration, _TypeID<PeriodUnit>::value, PeriodValue);
	}
	template<uint16_t Times>
	struct UntilTimes : RepeatEvery {
		UntilTimes(Process& Container)
		  : RepeatEvery(Container) {
		}
		void Restart() override {
			_RepeatEvery<Content>::Restart();
			_TimedModule::Timer->RepeatEvery(PeriodUnit{ PeriodValue }, _RepeatEvery<Content>::RepeatCallback, Times, FinishCallback);
		}
		bool Start(std::move_only_function<void() const> const& FC) override {
			if (!Times || Module::Container.TrialsDone.size())
				return false;
			FinishCallback = [this, &FC]() {
				_TimedModule::UnregisterTimer();
				FC();
			};
			Restart();
			return true;
		}
		void WriteInfo(std::ostringstream& InfoStream) const override {
			InfoWrite(InfoStream, UID::Type_Struct, 4, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Period, _TypeID<PeriodUnit>::value, PeriodValue, UID::Field_Times, UID::Type_UInt16, Times);
		}

	protected:
		std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	};
	template<typename Duration>
	using UntilDuration = _RepeatEvery_Constant_UntilDuration<PeriodUnit, PeriodValue, Content, Duration>;
};
template<typename PeriodUnit, uint16_t PeriodValue, typename Content, typename DurationUnit, uint16_t DurationValue>
struct _RepeatEvery_Constant_UntilDuration<PeriodUnit, PeriodValue, Content, ConstantDuration<DurationUnit, DurationValue>> : RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content> {
	_RepeatEvery_Constant_UntilDuration(Process& Container)
	  : RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content>(Container) {
	}
	void Restart() override {
		_RepeatEvery<Content>::Restart();
		_TimedModule::Timer->RepeatEvery(PeriodUnit{ PeriodValue }, _RepeatEvery<Content>::RepeatCallback, DurationUnit{ DurationValue }, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 4, UID::Field_ID, UID::Type_UID, _RepeatEvery<Content>::ID, UID::Field_Content, UID::Type_Pointer, &Content::ID, UID::Field_Period, _TypeID<PeriodUnit>::value, PeriodValue, UID::Field_Duration, _TypeID<DurationUnit>::value, DurationValue);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
};
template<typename PeriodUnit, uint16_t PeriodValue, typename Content>
struct _RepeatEvery_Constant_UntilDuration<PeriodUnit, PeriodValue, Content, InfiniteDuration> : RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content> {
	_RepeatEvery_Constant_UntilDuration(Process& Container)
	  : RepeatEvery<ConstantDuration<PeriodUnit, PeriodValue>, Content>(Container) {
	}
};
template<typename ContentA, typename ContentB>
struct _DoubleRepeat : _TimedModule {
	_DoubleRepeat(Process& Container)
	  : _TimedModule(Container) {
	}
	void Abort() override {
		ContentAPtr->Abort();
		ContentBPtr->Abort();
		_TimedModule::Abort();
	}
	void Restart() override {
		if (Timer) {
			ContentAPtr->Abort();
			ContentBPtr->Abort();
		} else
			Timer = Module::Container.AllocateTimer();
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		// 默认的无限重复，不需要FinishCallback。
		Restart();
		return true;
	}
	static constexpr UID ID = UID::Module_DoubleRepeat;

protected:
	ContentA* const ContentAPtr = Module::Container.LoadModule<ContentA>();
	std::move_only_function<void() const> const RepeatCallbackA = _EmptyStart{ ContentAPtr };
	ContentB* const ContentBPtr = Module::Container.LoadModule<ContentB>();
	std::move_only_function<void() const> const RepeatCallbackB = _EmptyStart{ ContentBPtr };
};
template<typename PeriodA, typename ContentA, typename PeriodB, typename ContentB, typename Duration>
struct _DoubleRepeat_Random_UntilDuration;
/*
等待时间A后，执行一次模块A；再等待时间B后，执行一次模块B，然后循环。周期总时长就是两个时间之和，不等待模块本身的执行时间。
此模块为固定周期的高频循环优化，比组合使用Repeat和Delay效率更高更方便。但是，一旦此模块开始，周期长度即固定。如果采用随机周期值，那个周期值将在模块开始时确定，后续再改变随机变量值也不会改变此模块的周期，除非重启。因此，不能使用此模块实现每个周期时长随机。要实现此效果，请组合使用Repeat和Delay模块。
*/
template<typename PeriodA, typename ContentA, typename PeriodB, typename ContentB>
struct DoubleRepeat : _DoubleRepeat<ContentA, ContentB> {
	template<uint16_t Times>
	struct UntilTimes : DoubleRepeat {
		UntilTimes(Process& Container)
		  : DoubleRepeat(Container) {
		}
		void Restart() override {
			_DoubleRepeat<ContentA, ContentB>::Restart();
			_TimedModule::Timer->DoubleRepeat(DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodAPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodBPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB, Times, FinishCallback);
		}
		bool Start(std::move_only_function<void() const> const& FC) override {
			if (!Times || Module::Container.TrialsDone.size())
				return false;
			FinishCallback = [this, &FC]() {
				_TimedModule::UnregisterTimer();
				FC();
			};
			Restart();
			return true;
		}
		void WriteInfo(std::ostringstream& InfoStream) const override {
			InfoWrite(InfoStream, UID::Type_Struct, 6, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, UID::Type_Pointer, &PeriodA::ID, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, UID::Type_Pointer, &PeriodB::ID, UID::Field_Times, UID::Type_UInt16, Times);
		}

	protected:
		std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	};
	template<typename Duration>
	using UntilDuration = _DoubleRepeat_Random_UntilDuration<PeriodA, ContentA, PeriodB, ContentB, Duration>;
	DoubleRepeat(Process& Container)
	  : _DoubleRepeat<ContentA, ContentB>(Container) {}
	void Restart() override {
		_DoubleRepeat<ContentA, ContentB>::Restart();
		_TimedModule::Timer->DoubleRepeat(DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodAPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodBPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB);
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 5, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, UID::Type_Pointer, &PeriodA::ID, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, UID::Type_Pointer, &PeriodB::ID);
	}

protected:
	PeriodA const* const PeriodAPtr = Module::Container.LoadModule<PeriodA>();
	PeriodB const* const PeriodBPtr = Module::Container.LoadModule<PeriodB>();
};
template<typename PeriodA, typename ContentA, typename PeriodB, typename ContentB, typename Duration>
struct _DoubleRepeat_Random_UntilDuration : DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB> {
	_DoubleRepeat_Random_UntilDuration(Process& Container)
	  : DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>(Container) {
	}
	void Restart() override {
		_DoubleRepeat<ContentA, ContentB>::Restart();
		_TimedModule::Timer->DoubleRepeat(DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodAPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodBPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB, DurationPtr->Current, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 6, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, UID::Type_Pointer, &PeriodA::ID, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, UID::Type_Pointer, &PeriodB::ID, UID::Field_Duration, UID::Type_Pointer, &Duration::ID);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	Duration const* const DurationPtr = Module::Container.LoadModule<Duration>();
};
template<typename PeriodA, typename ContentA, typename PeriodB, typename ContentB, typename Unit, uint16_t Value>
struct _DoubleRepeat_Random_UntilDuration<PeriodA, ContentA, PeriodB, ContentB, ConstantDuration<Unit, Value>> : DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB> {
	_DoubleRepeat_Random_UntilDuration(Process& Container)
	  : DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>(Container) {
	}
	void Restart() override {
		_DoubleRepeat<ContentA, ContentB>::Restart();
		_TimedModule::Timer->DoubleRepeat(DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodAPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>::PeriodBPtr->Current, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB, Unit{ Value }, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 6, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, UID::Type_Pointer, &PeriodA::ID, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, UID::Type_Pointer, &PeriodB::ID, UID::Field_Duration, _TypeID<Unit>::value, Value);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
};
template<typename PeriodA, typename ContentA, typename PeriodB, typename ContentB>
struct _DoubleRepeat_Random_UntilDuration<PeriodA, ContentA, PeriodB, ContentB, InfiniteDuration> : DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB> {
	_DoubleRepeat_Random_UntilDuration(Process& Container)
	  : DoubleRepeat<PeriodA, ContentA, PeriodB, ContentB>(Container) {
	}
};
template<typename PeriodUnitA, uint16_t PeriodValueA, typename ContentA, typename PeriodUnitB, uint16_t PeriodValueB, typename ContentB, typename Duration>
struct _DoubleRepeat_Constant_UntilDuration : DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB> {
	_DoubleRepeat_Constant_UntilDuration(Process& Container)
	  : DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB>(Container) {
	}
	void Restart() override {
		_DoubleRepeat<ContentA, ContentB>::Restart();
		_TimedModule::Timer->DoubleRepeat(PeriodUnitA{ PeriodValueA }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, PeriodUnitB{ PeriodValueB }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB, DurationPtr->Current, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 6, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, _TypeID<PeriodUnitA>::value, PeriodValueA, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, _TypeID<PeriodUnitB>::value, PeriodValueB, UID::Field_Duration, UID::Type_Pointer, &Duration::ID);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	Duration const* const DurationPtr = Module::Container.LoadModule<Duration>();
};
template<typename PeriodUnitA, uint16_t PeriodValueA, typename ContentA, typename PeriodUnitB, uint16_t PeriodValueB, typename ContentB>
struct DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB> : _DoubleRepeat<ContentA, ContentB> {
	DoubleRepeat(Process& Container)
	  : _DoubleRepeat<ContentA, ContentB>(Container) {}
	void Restart() override {
		_DoubleRepeat<ContentA, ContentB>::Restart();
		_TimedModule::Timer->DoubleRepeat(PeriodUnitA{ PeriodValueA }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, PeriodUnitB{ PeriodValueB }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB);
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 5, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, _TypeID<PeriodUnitA>::value, PeriodValueA, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, _TypeID<PeriodUnitB>::value, PeriodValueB);
	}
	template<uint16_t Times>
	struct UntilTimes : DoubleRepeat {
		UntilTimes(Process& Container)
		  : DoubleRepeat(Container) {
		}
		void Restart() override {
			_DoubleRepeat<ContentA, ContentB>::Restart();
			_TimedModule::Timer->DoubleRepeat(PeriodUnitA{ PeriodValueA }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, PeriodUnitB{ PeriodValueB }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB, Times, FinishCallback);
		}
		bool Start(std::move_only_function<void() const> const& FC) override {
			if (!Times || Module::Container.TrialsDone.size())
				return false;
			FinishCallback = [this, &FC]() {
				_TimedModule::UnregisterTimer();
				FC();
			};
			Restart();
			return true;
		}
		void WriteInfo(std::ostringstream& InfoStream) const override {
			InfoWrite(InfoStream, UID::Type_Struct, 6, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, _TypeID<PeriodUnitA>::value, PeriodValueA, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, _TypeID<PeriodUnitB>::value, PeriodValueB, UID::Field_Times, UID::Type_UInt16, Times);
		}

	protected:
		std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
	};
	template<typename Duration>
	using UntilDuration = _DoubleRepeat_Constant_UntilDuration<PeriodUnitA, PeriodValueA, ContentA, PeriodUnitB, PeriodValueB, ContentB, Duration>;
};
template<typename PeriodUnitA, uint16_t PeriodValueA, typename ContentA, typename PeriodUnitB, uint16_t PeriodValueB, typename ContentB, typename DurationUnit, uint16_t DurationValue>
struct _DoubleRepeat_Constant_UntilDuration<PeriodUnitA, PeriodValueA, ContentA, PeriodUnitB, PeriodValueB, ContentB, ConstantDuration<DurationUnit, DurationValue>> : DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB> {
	_DoubleRepeat_Constant_UntilDuration(Process& Container)
	  : DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB>(Container) {
	}
	void Restart() override {
		_DoubleRepeat<ContentA, ContentB>::Restart();
		_TimedModule::Timer->DoubleRepeat(PeriodUnitA{ PeriodValueA }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackA, PeriodUnitB{ PeriodValueB }, _DoubleRepeat<ContentA, ContentB>::RepeatCallbackB, DurationUnit{ DurationValue }, FinishCallback);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		FinishCallback = [this, &FC]() {
			_TimedModule::UnregisterTimer();
			FC();
		};
		Restart();
		return true;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 6, UID::Field_ID, UID::Type_UID, _DoubleRepeat<ContentA, ContentB>::ID, UID::Field_ContentA, UID::Type_Pointer, &ContentA::ID, UID::Field_PeriodA, _TypeID<PeriodUnitA>::value, PeriodValueA, UID::Field_ContentB, UID::Type_Pointer, &ContentB::ID, UID::Field_PeriodB, _TypeID<PeriodUnitB>::value, PeriodValueB, UID::Field_Duration, _TypeID<DurationUnit>::value, DurationValue);
	}

protected:
	std::move_only_function<void() const> FinishCallback = _TimedModule::_UnregisterTimer{ this };
};
template<typename PeriodUnitA, uint16_t PeriodValueA, typename ContentA, typename PeriodUnitB, uint16_t PeriodValueB, typename ContentB>
struct _DoubleRepeat_Constant_UntilDuration<PeriodUnitA, PeriodValueA, ContentA, PeriodUnitB, PeriodValueB, ContentB, InfiniteDuration> : DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB> {
	_DoubleRepeat_Constant_UntilDuration(Process& Container)
	  : DoubleRepeat<ConstantDuration<PeriodUnitA, PeriodValueA>, ContentA, ConstantDuration<PeriodUnitB, PeriodValueB>, ContentB>(Container) {
	}
};
struct _InstantaneousModule : Module {
	_InstantaneousModule(Process& Container)
	  : Module(Container) {
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.empty())
			Restart();
		return false;
	}
};
template<typename Target>
struct ModuleAbort : _InstantaneousModule {
	ModuleAbort(Process& Container)
	  : _InstantaneousModule(Container) {
	}
	void Restart() override {
		TargetPtr->Abort();
	}
	static constexpr UID ID = UID::Module_Abort;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID);
	}

protected:
	Module* const TargetPtr = Module::Container.LoadModule<Target>();
};
template<typename Target>
struct ModuleRestart : _InstantaneousModule {
	ModuleRestart(Process& Container)
	  : _InstantaneousModule(Container) {
	}
	void Restart() override {
		TargetPtr->Restart();
	}
	static constexpr UID ID = UID::Module_Restart;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID);
	}

protected:
	Module* const TargetPtr = Module::Container.LoadModule<Target>();
};
template<typename Target>
struct ModuleRandomize : _InstantaneousModule {
	ModuleRandomize(Process& Container)
	  : _InstantaneousModule(Container) {
	}
	void Restart() override {
		TargetPtr->Randomize();
	}
	static constexpr UID ID = UID::Module_Randomize;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID);
	}

protected:
	IRandom* const TargetPtr = reinterpret_cast<IRandom*>(Module::Container.LoadModule<Target>());
};
template<uint8_t Pin, bool HighOrLow>
struct DigitalWrite : _InstantaneousModule {
	DigitalWrite(Process& Container)
	  : _InstantaneousModule(Container) {
		Quick_digital_IO_interrupt::PinMode<Pin, OUTPUT>();
	}
	void Restart() override {
		Quick_digital_IO_interrupt::DigitalWrite<Pin, HighOrLow>();
	}
	static constexpr UID ID = UID::Module_DigitalWrite;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, ID, UID::Field_Pin, UID::Type_UInt8, Pin, UID::Field_HighOrLow, UID::Type_Bool, HighOrLow);
	}
};
// 此模块可以用ModuleAbort停止监视
template<uint8_t Pin, typename Monitor>
struct MonitorPin : _InstantaneousModule {
	MonitorPin(Process& Container)
	  : _InstantaneousModule(Container) {
		Quick_digital_IO_interrupt::PinMode<Pin, INPUT>();
	}
	PinListener* Listener = nullptr;
	void Abort() override {
		if (Listener) {
			Module::Container.UnregisterInterrupt(Listener);
			Listener = nullptr;
		}
	}
	void Restart() override {
		Abort();
		Listener = Module::Container.RegisterInterrupt(Pin, [MonitorPtr = MonitorPtr]() {
			MonitorPtr->Start(_EmptyCallback);
		});
	}
	static constexpr UID ID = UID::Module_MonitorPin;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, ID, UID::Field_Pin, UID::Type_UInt8, Pin, UID::Field_Monitor, UID::Type_Pointer, &Monitor::ID);
	}

protected:
	Monitor* const MonitorPtr = Module::Container.LoadModule<Monitor>();
};
template<UID Message>
struct SerialMessage : _InstantaneousModule {
	SerialMessage(Process& Container)
	  : _InstantaneousModule(Container) {
	}
	void Restart() override {
		SerialStream.AsyncInvoke(static_cast<uint8_t>(UID::PortC_Signal), &Container, Message);
	}
	static constexpr UID ID = UID::Module_SerialMessage;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Message, UID::Type_UID, Message);
	}
};
// 被Async包装的延时模块将异步执行，即不等待其结束直接返回继续。但是，对其调用Abort仍可以放弃内容模块。
template<typename Content>
struct Async : _InstantaneousModule {
	Async(Process& Container)
	  : _InstantaneousModule(Container) {
	}
	void Abort() override {
		ContentPtr->Abort();
	}
	void Restart() override {
		Abort();
		ContentPtr->Start(_EmptyCallback);
	}
	static constexpr UID ID = UID::Module_Async;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Content, UID::Type_Pointer, &Content::ID);
	}

protected:
	Content* const ContentPtr = Module::Container.LoadModule<Content>();
};
// 回合是断线重连后自动恢复的最小单元，并且在回合开始时向串口发送信号
template<UID TrialID, typename Content>
struct Trial : Module {
	Trial(Process& Container)
	  : Module(Container) {
	}
	void Abort() override {
		ContentPtr->Abort();
	}
	void Restart() override {
		_Restart();
		ContentPtr->Start(*FinishCallback);
	}
	static constexpr UID ID = TrialID;
	bool Start(std::move_only_function<void() const> const& FC) override {
		auto const It = Module::Container.TrialsDone.find(TrialID);
		if (It != Module::Container.TrialsDone.end())
			if (It->second) {
				if (!--It->second)
					Module::Container.TrialsDone.erase(It);
				return false;
			} else
				Module::Container.TrialsDone.erase(It);
		_Restart();
		if (ContentPtr->Start(FC)) {
			FinishCallback = &FC;
			return true;
		}
		return false;
	}
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 2, UID::Field_ID, UID::Type_UID, ID, UID::Field_Content, UID::Type_Pointer, &Content::ID);
	}

protected:
	Content* const ContentPtr = Module::Container.LoadModule<Content>();
	std::move_only_function<void() const> const* FinishCallback = &_EmptyCallback;
	void _Restart() {
		Abort();
		SerialStream.AsyncInvoke(static_cast<uint8_t>(UID::PortC_TrialStart), &Container, TrialID);
	}
};
// 将一个清理模块附加到目标模块上，开始、重启或终止此模块前都将先执行清理模块，但目标模块正常结束时则不会清理。清理模块一般应是瞬时的，如果有延时操作则不会等待其完成。
template<typename Target, typename Cleaner>
struct CleanWhenAbort : Module {
	CleanWhenAbort(Process& Container)
	  : Module(Container) {
	}
	void Abort() override {
		_Abort();
		Module::Container.ExtraCleaners.erase(&StartCleaner);
	}
	void Restart() override {
		_Abort();
		if (TargetPtr->Start(TargetCallback))
			Module::Container.ExtraCleaners.insert(&StartCleaner);
	}
	bool Start(std::move_only_function<void() const> const& FC) override {
		if (Module::Container.TrialsDone.size())
			return false;
		_Abort();
		if (TargetPtr->Start(TargetCallback)) {
			FinishCallback = &FC;
			Module::Container.ExtraCleaners.insert(&StartCleaner);
			return true;
		}
		return false;
	}
	static constexpr UID ID = UID::Module_CleanWhenAbort;
	void WriteInfo(std::ostringstream& InfoStream) const override {
		InfoWrite(InfoStream, UID::Type_Struct, 3, UID::Field_ID, UID::Type_UID, ID, UID::Field_Target, UID::Type_Pointer, &Target::ID, UID::Field_Cleaner, UID::Type_Pointer, &Cleaner::ID);
	}

protected:
	Target* const TargetPtr = Module::Container.LoadModule<Target>();
	std::move_only_function<void() const> const* FinishCallback = &_EmptyCallback;
	std::move_only_function<void() const> const TargetCallback = [this]() {
		Module::Container.ExtraCleaners.erase(&StartCleaner);
		(*FinishCallback)();
	};
	std::move_only_function<void() const> const StartCleaner = [CleanerPtr = Module::Container.LoadModule<Cleaner>()]() {
		CleanerPtr->Start(_EmptyCallback);
	};
	void _Abort() {
		TargetPtr->Abort();
		StartCleaner();
	}
};
template<typename TModule>
bool Session(Process* P, uint16_t Times) {
	return P->Start<TModule>(Times);
};
#define Pin static constexpr uint8_t