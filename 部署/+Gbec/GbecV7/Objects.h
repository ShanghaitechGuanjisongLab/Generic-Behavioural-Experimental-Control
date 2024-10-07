#pragma once
#include "UID.h"
#include "Async_stream_IO.hpp"
#include <unordered_map>
#include <random>
#include <dynarray>
#include <span>
#pragma pack(push, 1)
struct StackFrame
{
	uint8_t StackLevel;
	UID Template;
	uint16_t Progress;
};
struct CallbackMessage
{
	UID Exception;
	std::vector<StackFrame> StackTrace;
};
struct ExceptionProgress
{
	UID Type = UID::Progress_Exception;
	UID Exception;
	constexpr ExceptionProgress(UID Exception) : Exception(Exception) {}
};
#pragma pack(pop)
// 所有派生类应当支持无参构造，以便在ObjectCreators中注册。
struct Object
{
	/*开始Object，指定用于接收进度信息的远程端口和堆栈层级。不支持进度的Object可以忽略这两个参数。Trial是引用类型，因此被调用的Object可以借此向调用方返回进度信息。
	如果任务在本次调用的上下文中即完成，返回Exception_ObjectFinished，并且不应再调用FinishCallback。如果任务依赖某个延迟发生的条件才算完成，本次调用应返回Exception_StillRunning，并在任务彻底完成时调用FinishCallback。
	调用方应根据返回值决定是否要等待FinishCallback被调用。如果返回Exception_ObjectFinished，应立即调度后续任务，可以认为FinishCallback不会被调用。如果返回Exception_StillRunning，应在FinishCallback中调度后续任务。
	FinishCallback通常会被中断调用，因此可能会与其访问相同资源的过程通常应当禁用中断。
	如果
	*/
	virtual UID Start() { return UID::Exception_Success; }
	virtual UID Restore(std::span<const char> ProgressInfo) { return UID::Exception_Success; }
	// 支持重复的Object应override此方法，重复执行Times次，返回Exception_Success。
	virtual UID Repeat(uint16_t Times) { return UID::Exception_Success; }
	// 支持暂停的Object应override此方法，暂停当前执行，返回Exception_Success。调用方应检查返回值判断当前Object是否支持暂停，在不支持暂停的情况下采取其它措施实现暂停。
	virtual UID Pause() { return UID::Exception_MethodNotSupported; }
	// 支持暂停的Object应override此方法，继续上次暂停的执行。
	virtual UID Continue() { return UID::Exception_MethodNotSupported; }
	// 支持废弃的Object应override此方法，废弃当前执行，返回Exception_Success。调用方应检查返回值判断当前Object是否支持废弃，在不支持废弃的情况下采取其它措施实现废弃。
	virtual UID Abort() { return UID::Exception_MethodNotSupported; }
	// 支持获取信息的Object应override此方法
	virtual UID GetInformation(std::dynarray<char>& Container) { return UID::Exception_MethodNotSupported; }
	virtual ~Object() {}

protected:
	const uint8_t ProgressPort;
	const uint8_t StackLevel;
	Object(uint8_t ProgressPort, uint8_t StackLevel) : ProgressPort(ProgressPort), StackLevel(StackLevel) {}
	virtual void FinishCallback(std::unique_ptr<CallbackMessage> Message) = 0;
};
struct RootObject : Object
{
	Object* Child;
	RootObject(uint8_t ProgressPort) : Object(ProgressPort, 0) {}
	virtual ~RootObject() { delete Child; }
	UID Start() override
	{
		return Child->Start();
	};
	UID Restore(std::span<const char> ProgressInfo) override
	{
		return Child->Restore(ProgressInfo);
	}
	UID Repeat(uint16_t Times) override
	{
		return Child->Repeat(Times);
	}
	UID Pause() override
	{
		return Child->Pause();
	}
	UID Continue() override
	{
		return Child->Continue();
	}
	UID Abort() override
	{
		return Child->Abort();
	}
	UID GetInformation(std::dynarray<char>& Container) override
	{
		return Child->GetInformation(Container);
	}

protected:
	void FinishCallback(std::unique_ptr<CallbackMessage> Message) override
	{
		Async_stream_IO::Send([Message = std::move(Message)](const std::move_only_function<void(const void* Message, uint8_t Size) const>& MessageSender)
			{
				ExceptionProgress Header(Message->Exception);
				if (Header.Exception == UID::Exception_Success)
					MessageSender(&Header, sizeof(ExceptionProgress));
				else
				{
					const uint8_t Size = sizeof(ExceptionProgress) + sizeof(StackFrame) * Message->StackTrace.size();
					std::unique_ptr<char[]> Buffer = std::make_unique_for_overwrite<char[]>(Size);
					*reinterpret_cast<ExceptionProgress*>(Buffer.get()) = Header;
					std::copy(Message->StackTrace.cbegin(), Message->StackTrace.cend(), reinterpret_cast<StackFrame*>(reinterpret_cast<ExceptionProgress*>(Buffer.get()) + 1));
					MessageSender(Buffer.get(), Size);
				}
			}, ProgressPort);
	}
};
struct ChildObject : Object
{
	Object* const Parent;
};
template <typename ObjectType>
inline Object* New()
{
	return new ObjectType;
}
#define OverrideGetInformation                                         \
	UID GetInformation(std::dynarray<char> &Container) override        \
	{                                                                  \
		Container = std::dynarray<char>(sizeof(Information));          \
		memcpy_P(Container.data(), &Information, sizeof(Information)); \
		return UID::Exception_Success;                                 \
	}
template <typename T>
struct TypeID;
template <>
struct TypeID<bool>
{
	static constexpr UID value = UID::Type_Bool;
};
template <>
struct TypeID<uint8_t>
{
	static constexpr UID value = UID::Type_UInt8;
};
template <>
struct TypeID<uint16_t>
{
	static constexpr UID value = UID::Type_UInt16;
};
template <>
struct TypeID<UID>
{
	static constexpr UID value = UID::Type_UID;
};
#pragma pack(push, 1)
template <typename T, typename... Ts>
struct InfoArray
{
	static constexpr UID ID = UID::Type_Array;
	uint8_t Number = sizeof...(Ts) + 1;
	UID Type = TypeID<typename T::value_type>::value;
	typename T::value_type Array[sizeof...(Ts) + 1] = { T::value, Ts::value... };
};
template <typename T, typename... Ts>
struct CellArray
{
	UID Type = TypeID<T>::value;
	T Value;
	CellArray<Ts...> Values;
	constexpr CellArray(T Value, Ts... Values)
		: Value(Value), Values(CellArray<Ts...>(Values...)) {}
};
template <typename T>
struct CellArray<T>
{
	UID Type = TypeID<T>::value;
	T Value;
	constexpr CellArray(T Value)
		: Value(Value) {}
};
template <typename... T>
struct InfoCell
{
	static constexpr UID ID = UID::Type_Cell;
	uint8_t NumCells = 0;
};
template <typename T, typename... Ts>
struct InfoCell<T, Ts...>
{
	static constexpr UID ID = UID::Type_Cell;
	uint8_t NumCells = sizeof...(Ts) + 1;
	CellArray<T, Ts...> Values;
	constexpr InfoCell(T Value, Ts... Values)
		: Values(CellArray<T, Ts...>(Value, Values...)) {}
};
// 只有主模板能推断，特化模板必须加推断向导
template <typename T, typename... Ts>
InfoCell(T, Ts...) -> InfoCell<T, Ts...>;
template <typename TName, typename TValue, typename... Ts>
struct InfoFields
{
	TName Name;
	UID Type = TypeID<TValue>::value;
	TValue Value;
	InfoFields<Ts...> FollowingFields;
	constexpr InfoFields(TName Name, TValue Value, Ts... NameValues)
		: Name(Name), Value(Value), FollowingFields(InfoFields<Ts...>(NameValues...)) {}
};
template <typename TName, typename TValue>
struct InfoFields<TName, TValue>
{
	TName Name;
	UID Type = TypeID<TValue>::value;
	TValue Value;
	constexpr InfoFields(TName Name, TValue Value)
		: Name(Name), Value(Value) {}
};
template <typename... Ts>
struct InfoStruct
{
	constexpr static UID ID = UID::Type_Struct;
	uint8_t NumFields = 0;
};
template <typename T, typename... Ts>
struct InfoStruct<UID, T, Ts...>
{
	constexpr static UID ID = UID::Type_Struct;
	uint8_t NumFields = sizeof...(Ts) / 2 + 1;
	InfoFields<UID, T, Ts...> Fields;
	constexpr InfoStruct(UID Name, T Value, Ts... NameValues)
		: Fields(InfoFields<UID, T, Ts...>(Name, Value, NameValues...)) {}
};
// 只有主模板能推断，特化模板必须加推断向导
template <typename... Ts>
InfoStruct(UID, Ts...) -> InfoStruct<UID, Ts...>;
#pragma pack(pop)
// 顺序依次执行所有的Object
template <typename... ObjectType>
struct Sequential : Object
{
	Sequential(uint8_t StackLevel, ICallback* Parent = nullptr) : Object(StackLevel, Parent), SubObjects{ new ObjectType(StackLevel + 1, this)... } {}
	UID Start(uint8_t ProgressPort) override
	{
		if (State != UID::State_ObjectIdle)
			return UID::Exception_ObjectNotIdle;
		this->ProgressPort = ProgressPort;
		Progress = 0;
		const UID Result = Iterate();
		if (Result == UID::Exception_StillRunning)
			State = UID::State_ObjectRunning;
		return Result;
	}
	UID Restore(uint8_t ProgressPort, std::span<const char> ProgressInfo) override
	{
		if (State != UID::State_ObjectIdle)
			return UID::Exception_ObjectNotIdle;
		this->ProgressPort = ProgressPort;
		Progress = *reinterpret_cast<const uint8_t*>(ProgressInfo.data());
		if (Progress >= sizeof...(ObjectType))
			return UID::Exception_ObjectFinished;
		ProgressInfo = ProgressInfo.subspan(sizeof(Progress));
		if (ProgressInfo.empty())
		{
			const UID Result = Iterate();
			if (Result == UID::Exception_StillRunning)
			{
				this->FinishCallback = std::move(FinishCallback);
				State = UID::State_ObjectRunning;
			}
			return Result;
		}
		else
		{
			const UID Result = SubObjects[Progress]->Restore(ProgressPort, ProgressInfo, [this]()
				{ SubCallback(); });
		}
	}
	virtual ~Sequential()
	{
		Abort();
		for (Object* O : SubObjects)
			delete O;
	}
	OverrideGetInformation;

protected:
	uint8_t ProgressPort;
	uint8_t Progress;
	Object* SubObjects[sizeof...(ObjectType)];
	static constexpr auto Information PROGMEM = InfoStruct(UID::Property_TemplateID, UID::TemplateID_Sequential, UID::Property_Subobjects, InfoCell(ObjectType::Information...));
	UID State = UID::State_ObjectIdle;
	void FinishCallback(UID Result) override
	{
		if (Result != UID::Exception_ObjectFinished)
		{
			State = UID::State_ObjectIdle;
			if (Parent)
				Parent->FinishCallback(Result);
			else
				Async_stream_IO::Send(ProgressMessage<UID>{StackLevel, UID::Progress_Exception, Result}, ProgressPort);
		}
		if (StackLevel < UINT8_MAX)
			Async_stream_IO::Send(ProgressMessage<uint8_t>{StackLevel, UID::Progress_Custom, Progress}, ProgressPort);
		if (Iterate() == UID::Exception_ObjectFinished)
		{
			State = UID::State_ObjectIdle;
			Parent->FinishCallback();
		}
	}
	UID Iterate()
	{
		while (Progress < sizeof...(ObjectType))
			switch (const UID Result = SubObjects[Progress]->Start(ProgressPort);)
			{
			default:
				return Result;
			case UID::Exception_StillRunning:
				return UID::Exception_StillRunning;
			case UID::Exception_ObjectFinished:
				Progress++;
			}
		return UID::Exception_ObjectFinished;
	}
};