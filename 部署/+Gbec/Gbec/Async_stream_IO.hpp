#pragma once
#include <Cpp_Standard_Library.h>
#include <memory>
#include <span>
#include <unordered_map>
#include <Arduino.h>
namespace Async_stream_IO {
	enum class Exception : uint8_t {
		Success = 0,
		Function_runtime_error = 1,
		Port_idle = 2,
		Port_occupied = 3,
		Argument_message_incomplete = 4,
		Corrupted_object_received = 5,
		Message_received_on_allocated_port = 6,
	};
	struct _InterruptGuard {
		_InterruptGuard() {
			noInterrupts();
		}
		~_InterruptGuard() {
			if (InterruptEnabled)
				interrupts();
		}

	protected:
		bool const InterruptEnabled =
#ifdef ARDUINO_ARCH_AVR
			SREG & 1 << SREG_I
#endif
#ifdef ARDUINO_ARCH_SAM
			!__get_PRIMASK()
#endif
			;
	};
	//轻量化的消息发送同步器。对象构造后将禁用中断，保证消息连续发出。此对象生存期内，用户必须直接使用BaseStream.write()方法发送数据，而不能使用Send；此方法发送的所有数据会被字节串联并视为单条消息。在对象析构前，用户必须恰好已发送Length字节的数据，不能多也不能少，否则行为未定义。对象析构时中断会恢复到构造前的状态。
	struct SendSession : protected _InterruptGuard {
		SendSession(uint8_t Length, uint8_t ToPort, Stream& BaseStream);
	};
	template<size_t Plus, typename T>
	struct _PlusAndPrepend;
	template<size_t Plus, size_t... Values>
	struct _PlusAndPrepend<Plus, std::index_sequence<Values...>> {
		using type = std::index_sequence<0, Plus + Values...>;
	};

	template<typename T>
	struct _CumSum;
	template<size_t First, size_t... Rest>
	struct _CumSum<std::index_sequence<First, Rest...>> {
		using type = typename _PlusAndPrepend<First, typename _CumSum<std::index_sequence<Rest...>>::type>::type;
	};
	template<>
	struct _CumSum<std::index_sequence<>> {
		using type = std::index_sequence<>;
	};

	template<typename...>
	struct _TypesSize {
		static constexpr size_t value = 0;
	};
	template<typename Only>
	struct _TypesSize<Only> {
		static constexpr size_t value = sizeof(Only);
	};
	template<typename First, typename... Rest>
	struct _TypesSize<First, Rest...> {
		static constexpr size_t value = sizeof(First) + _TypesSize<Rest...>::value;
	};
	constexpr size_t ST = _TypesSize<int>::value;

	template<typename>
	struct _NotMof {
		using type = uint8_t;
	};
	template<typename T>
	struct _NotMof<std::move_only_function<T>> {};
	template<typename...>
	struct _FirstNotStream {
		static constexpr bool value = true;
	};
	static void _CallableAsStream_f(Stream&);
	template<typename T, typename = void>
	struct _CallableAsStream_s {
		static constexpr bool value = false;
	};
	template<typename T>
	struct _CallableAsStream_s<T, decltype(_CallableAsStream_f(*std::declval<T*>()))> {
		static constexpr bool value = true;
	};
	template<typename First, typename... Rest>
	struct _FirstNotStream<First, Rest...> {
		static constexpr bool value = !_CallableAsStream_s<First>::value;
	};
	template<typename T, typename = std::_FunctionSignature_t<T>>
	struct _FunctionTraits {
	};
	template<typename TFunction, typename TReturn, typename... TArgument>
	struct _FunctionTraits<TFunction, TReturn(TArgument...)const> {
		static constexpr size_t ArgumentsSize = _TypesSize<ArgumentType...>::value;
		template<typename = _CumSum<std::index_sequence<sizeof(ArgumentType)...>>::type>
		struct InvokeWithMemoryOffsets;
	};
	template<typename TFunction, typename TReturn, typename... TArgument>
	template<uint8_t... Offsets>
	struct _FunctionTraits<TFunction, TReturn(TArgument...) const>::InvokeWithMemoryOffsets<std::index_sequence<Offsets...>> {
#pragma pack(push, 1)
		struct ReturnMessage {
			Exception Result;
			TReturn ReturnValue;
		};
#pragma pack(pop)
		static ReturnMessage Invoke(TFunction const& Function, char const* Arguments) {
			return { Exception::Success,Function(*reinterpret_cast<TArgument const*>(Arguments + Offsets)...) };
		}
	};
	template<typename TFunction, typename... TArgument>
	template<uint8_t... Offsets>
	struct _FunctionTraits<TFunction, void(TArgument...) const>::InvokeWithMemoryOffsets<std::index_sequence<Offsets...>> {
		static Exception Invoke(TFunction const& Function, char const* Arguments) {
			Function(*reinterpret_cast<TArgument const*>(Arguments + Offsets)...);
			return Exception::Success;
		}
	};
	struct AsyncStream {
		Stream& BaseStream;
		//构造后，不得另外调用BaseStream.setTimeout，否则行为未定义。
		AsyncStream(Stream& BaseStream)
			: BaseStream(BaseStream) {
			BaseStream.setTimeout(-1);
		}

		/*一般应在loop中调用此方法。它将实际执行所有排队中的监听器。
					所有被委托的Stream在此方法中实际被读。那些Stream的所有操作都应当托管给此方法，用户不应再直接访问那些Stream，否则行为未定义。
					用户在本库其它方法中提供的回调，将被此方法实际调用。调用这些函数时，中断将处于启用状态。即使在调用此方法前禁用了中断，也会被此方法重新启用。即使在回调中禁用了中断，也仅适用于本次回调，返回后又将重新启用中断。这是因为串口操作需要中断支持，因此必须启用中断。
					 */
		void ExecuteTransactionsInQueue();


		// 向远程ToPort发送指定Length的Message
		void Send(const void* Message, uint8_t Length, uint8_t ToPort) const;
		// 向远程ToPort发送指定Message。这个Message应当可以直接将底层字节发送给串口
		template<typename T, std::enable_if_t<!std::invocable<T> _CSL_Parentheses11, int> = 0>
		inline void Send(const T& Message, uint8_t ToPort) const {
			Send(&Message, sizeof(T), ToPort);
		}

		//分配一个空闲端口号。该端口号将保持被占用状态，不再参与自动分配，直到调用ReleasePort。
		uint8_t AllocatePort();
		//检查指定端口Port是否被占用。
		bool PortOccupied(uint8_t Port) const {
			_InterruptGuard const _;
			return _Listeners.contains(Port);
		}
		//立即释放指定本地端口，取消任何监听或绑定函数。
		void ReleasePort(uint8_t Port) {
			_InterruptGuard const _;
			_Listeners.erase(Port);
		}

		/*持续监听本地FromPort端口。当远程传来指向FromPort的消息时，调用 void Callback(uint8_t MessageSize)，由用户负责从BaseStream读取消息内容。在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
		如果FromPort已被监听，将覆盖。
		此监听是持续性的，每次收到消息都会重复调用Callback。如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort。
		此方法保证Callback被调用时中断处于启用状态。
		*/
		template<typename T>
		void Listen(T&& Callback, uint8_t FromPort) {
			_InterruptGuard const _;
			_Listeners[FromPort] = std::forward<T>(Callback);
		}
		/*自动分配一个空闲端口并持续监听。当远程传来指向该端口的消息时，调用 void Callback(uint8_t MessageSize)，由用户负责从BaseStream读取消息内容。在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。返回分配的端口号。
		此监听是持续性的，每次收到消息都会重复调用Callback。如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort。
		此方法保证Callback被调用时中断处于启用状态。
		*/
		template<typename T>
		uint8_t Listen(T&& Callback) {
			_InterruptGuard const _;
			uint8_t const FromPort = _AllocatePort();
			_Listeners[FromPort] = std::forward<T>(Callback);
			return FromPort;
		}

		/* 将任意可调用对象绑定到指定本地端口上作为服务，当收到消息时调用。如果端口被占用，将覆盖。
		远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Argument_message_incomplete。
		Function的参数和返回值（如果有）必须是平凡类型。
		此方法保证Function被调用时中断处于启用状态。
		使用ReleasePort可以取消绑定，释放端口。
		*/
		template<typename T>
		void BindFunctionToPort(T&& Function, uint8_t Port) {
			Listen([Function = std::forward<T>(Function), this](uint8_t MessageSize) {
				const std::unique_ptr<uint8_t[]> Arguments = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
				BaseStream->readBytes(Arguments.get(), MessageSize);
				Send(MessageSize == _FunctionTraits<T>::ArgumentsSize + sizeof(uint8_t) ? _FunctionTraits<T>::InvokeWithMemoryOffsets::Invoke(Function, reinterpret_cast<char*>(Arguments.get() + 1)) : Exception::Argument_message_incomplete, Arguments[0]);
				}, Port);
		}
		/* 将任意可调用对象绑定到自动分配的空闲端口上作为服务，当收到消息时调用。返回分配的端口号。
		远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Argument_message_incomplete。
		Function的参数和返回值（如果有）必须是平凡类型。
		此方法保证Function被调用时中断处于启用状态。
		使用ReleasePort可以取消绑定，释放端口。
		*/
		template<typename T>
		uint8_t BindFunctionToPort(T&& Function) {
			return Listen([Function = std::forward<T>(Function), this](uint8_t MessageSize) {
				const std::unique_ptr<uint8_t[]> Arguments = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
				BaseStream->readBytes(Arguments.get(), MessageSize);
				Send(MessageSize == _FunctionTraits<T>::ArgumentsSize + sizeof(uint8_t) ? _FunctionTraits<T>::InvokeWithMemoryOffsets::Invoke(Function, reinterpret_cast<char*>(Arguments.get() + 1)) : Exception::Argument_message_incomplete, Arguments[0]);
				});
		}
		/* 远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
		如果远程端口未被监听，Callback将不会被调用。
		如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
		如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
		ReturnType和ArgumentType必须是平凡类型。
		此方法保证Callback被调用时中断处于启用状态。
		*/
		template<typename TCallback, typename... TArgument>
		void RemoteInvoke(uint8_t RemotePort, uint8_t LocalPort, TCallback&& Callback, TArgument... Arguments) {
			Listen([LocalPort, Callback = std::forward<TCallback>(Callback),this](uint8_t MessageSize) {
				ReleasePort(LocalPort);
				const std::unique_ptr<char[]> Message = std::make_unique_for_overwrite<char[]>(MessageSize);
				BaseStream.readBytes(Message.get(), MessageSize);
				if (MessageSize < sizeof(Exception))
					Callback(Exception::Corrupted_object_received, {});
				else if (MessageSize < sizeof(Exception) + sizeof(ReturnType))
					Callback(*reinterpret_cast<const Exception*>(Message.get()), {});
				else
					Callback(*reinterpret_cast<const Exception*>(Message.get()), *reinterpret_cast<const ReturnType*>(reinterpret_cast<const Exception*>(Message.get()) + 1));
				}, LocalPort);
			SendSession const Session{ sizeof(uint8_t) + _TypesSize<TArgument...>::value, RemotePort, BaseStream };
			BaseStream.write(LocalPort);
			size_t const Written[] = { BaseStream.write(reinterpret_cast<char*>(&Arguments), sizeof(Arguments))... };
			return LocalPort;
		}
		/* 远程调用指定RemotePort上的函数，传入参数Arguments。当远程函数返回时，调用Callback，提供调用结果Result和返回值ReturnValue。
					如果远程端口未被监听，Callback将不会被调用。
					如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
					如果Result为Parameter_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
					ReturnType和ArgumentType必须是平凡类型。
					此方法保证Callback被调用时中断处于启用状态。
					*/
		template<typename CallbackType, typename... ArgumentType>
		inline typename _NotMof<CallbackType>::type RemoteInvoke(uint8_t RemotePort, const CallbackType& Callback, ArgumentType... Arguments) {
			return RemoteInvoke(RemotePort, std::move_only_function<std::_FunctionSignature_t<CallbackType>>(Callback), Arguments...);
		}
		std::unordered_map<uint8_t, std::move_only_function<void(uint8_t MessageSize) const>> _Listeners;
	protected:
		uint8_t _AllocatePort()const;
	};
}