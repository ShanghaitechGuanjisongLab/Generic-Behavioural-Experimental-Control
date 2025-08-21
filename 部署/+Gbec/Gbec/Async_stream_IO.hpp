#pragma once
#include <Cpp_Standard_Library.h>
#include <memory>
#include <unordered_map>
#include <Arduino.h>
namespace Async_stream_IO
{
	enum class Exception : uint8_t
	{
		Success,
		Argument_message_incomplete,
		Corrupted_object_received,
		Function_runtime_error,
		Message_received_on_allocated_port,
		Unlistened_port_received_message,
		Remote_function_have_no_return,
		Serial_not_respond_in_time,
	};
	// 构造此临时对象以禁用中断并保存状态。当前作用域退出时，此对象析构并恢复之前的中断状态。
	struct InterruptGuard
	{
		InterruptGuard()
		{
			noInterrupts();
		}
		~InterruptGuard()
		{
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
	// 轻量化的消息发送同步器。对象构造后将禁用中断，保证消息连续发出。此对象生存期内，用户必须用BaseStream.write或operator<<手动向基础流写入数据，而不能使用Send；此方法发送的所有数据会被字节串联并视为单条消息。在对象析构前，用户必须恰好已写入共计Length字节的数据，不能多也不能少，否则行为未定义。对象析构时中断会恢复到构造前的状态。此对象在BaseStream已被AsyncStream托管的情况下也允许正常使用。
	struct SendSession : protected InterruptGuard
	{
		Stream &BaseStream;
		SendSession(uint8_t Length, uint8_t ToPort, Stream &BaseStream);
		// 向基础流写入平凡对象。累计写入字节数不能超过构造SendSession时指定的Length。
		template <typename T>
		SendSession const &operator<<(T const &Value) const
		{
			BaseStream.write(reinterpret_cast<char const *>(&Value), sizeof(T));
			return *this;
		}
	};
	// 基于端口的异步读写流。端口的有效范围是0~254，255是无效端口号。
	class AsyncStream
	{
		template <size_t Plus, typename T>
		struct _PlusAndPrepend;
		template <size_t Plus, size_t... Values>
		struct _PlusAndPrepend<Plus, std::index_sequence<Values...>>
		{
			using type = std::index_sequence<0, Plus + Values...>;
		};

		template <typename T>
		struct _CumSum
		{
			using type = std::index_sequence<>;
		};
		template <size_t First, size_t... Rest>
		struct _CumSum<std::index_sequence<First, Rest...>>
		{
			using type = typename _PlusAndPrepend<First, typename _CumSum<std::index_sequence<Rest...>>::type>::type;
		};

		template <typename...>
		struct _TypesSize
		{
			static constexpr size_t value = 0;
		};
		template <typename First, typename... Rest>
		struct _TypesSize<First, Rest...>
		{
			static constexpr size_t value = sizeof(First) + _TypesSize<Rest...>::value;
		};

		template <typename T, typename = std::_FunctionSignature_t<T>>
		struct _FunctionTraits;
		template <typename TFunction, typename TReturn, typename... TArgument>
		struct _FunctionTraits<TFunction, TReturn(TArgument...) const>
		{
			static constexpr size_t ArgumentsSize = _TypesSize<TArgument...>::value;
			template <typename = typename _CumSum<std::index_sequence<sizeof(TArgument)...>>::type>
			struct InvokeWithMemoryOffsets;
			template <size_t... Offsets>
			struct InvokeWithMemoryOffsets<std::index_sequence<Offsets...>>
			{
#pragma pack(push, 1)
				struct ReturnMessage
				{
					Exception Result;
					TReturn ReturnValue;
				};
#pragma pack(pop)
				static ReturnMessage Invoke(TFunction const &Function, char const *Arguments)
				{
					return {Exception::Success, Function(*reinterpret_cast<TArgument const *>(Arguments + Offsets)...)};
				}
			};
		};
		template <typename TFunction, typename... TArgument>
		struct _FunctionTraits<TFunction, void(TArgument...) const>
		{
			static constexpr size_t ArgumentsSize = _TypesSize<TArgument...>::value;
			template <typename = typename _CumSum<std::index_sequence<sizeof(TArgument)...>>::type>
			struct InvokeWithMemoryOffsets;
			template <size_t... Offsets>
			struct InvokeWithMemoryOffsets<std::index_sequence<Offsets...>>
			{
				static Exception Invoke(TFunction const &Function, char const *Arguments)
				{
					Function(*reinterpret_cast<TArgument const *>(Arguments + Offsets)...);
					return Exception::Success;
				}
			};
		};

		uint8_t _AllocatePort() const;

		template <typename T, typename...>
		struct TypeIfValid
		{
			typedef T type;
		};

		std::unordered_map<uint8_t, std::move_only_function<void(uint8_t MessageSize) const>> _Listeners;

		template <typename T>
		struct FunctionListener
		{
			FunctionListener(T &&Function, AsyncStream const &Stream)
				: Function(std::forward<T>(Function)), Stream(Stream)
			{
			}
			void operator()(uint8_t MessageSize) const
			{
				if (MessageSize < sizeof(uint8_t))
				{
					if (sizeof(uint8_t) > 1)
						Stream.Skip(MessageSize);
					return;
				}
				uint8_t const ReturnPort = Stream.Read<uint8_t>();
				MessageSize -= sizeof(ReturnPort);
				if (MessageSize != _FunctionTraits<T>::ArgumentsSize)
				{
					Stream.Skip(MessageSize);
					if (ReturnPort < 255)
						Stream.Send(Exception::Argument_message_incomplete, ReturnPort);
					return;
				}
				const std::unique_ptr<uint8_t[]> Arguments = std::make_unique_for_overwrite<uint8_t[]>(MessageSize);
				Stream.BaseStream.readBytes(Arguments.get(), MessageSize);
				auto const ReturnMessage = _FunctionTraits<T>::template InvokeWithMemoryOffsets<>::Invoke(Function, reinterpret_cast<char const *>(Arguments.get()));
				if (ReturnPort < 255)
					Stream.Send(ReturnMessage, ReturnPort);
			}

		protected:
			T Function;
			AsyncStream const &Stream;
		};
		template <typename TCallback, typename = std::_FunctionSignature_t<TCallback>>
		struct CallbackListener
		{
			CallbackListener(TCallback &&Callback, AsyncStream &Stream, uint8_t LocalPort)
				: Callback(std::forward<TCallback>(Callback)), Stream(Stream), LocalPort(LocalPort)
			{
			}
			void operator()(uint8_t MessageSize) const
			{
				Stream.ReleasePort(LocalPort); // 此时PortForward已将本对象从_Listeners中移出到本地栈，因此释放端口不会释放本对象
				if (MessageSize == sizeof(Exception))
					Callback(Stream.Read<Exception>());
				else
				{
					Stream.Skip(MessageSize);
					Callback(Exception::Corrupted_object_received);
				}
			}

		protected:
			TCallback Callback;
			AsyncStream &Stream;
			uint8_t const LocalPort;
		};
		template <typename TCallback, typename TReturn>
		struct CallbackListener<TCallback, void(Exception, TReturn) const>
		{
			CallbackListener(TCallback &&Callback, AsyncStream &Stream, uint8_t LocalPort)
				: Callback(std::forward<TCallback>(Callback)), Stream(Stream), LocalPort(LocalPort)
			{
			}
			void operator()(uint8_t MessageSize) const
			{
				Stream.ReleasePort(LocalPort); // 此时PortForward已将本对象从_Listeners中移出到本地栈，因此释放端口不会释放本对象
				switch (MessageSize)
				{
				case sizeof(Exception):
					Callback(Stream.Read<Exception>(), {});
					break;
				case sizeof(Exception) + sizeof(TReturn):
				{
					Exception const E = Stream.Read<Exception>();
					Callback(E, Stream.Read<TReturn>());
				}
				break;
				default:
					Stream.Skip(MessageSize);
					Callback(Exception::Corrupted_object_received, {});
					break;
				}
			}

		protected:
			TCallback Callback;
			AsyncStream &Stream;
			uint8_t const LocalPort;
		};
#pragma pack(push, 1)
		// 只有两字节，没必要按引用传递
		struct AsioHeader
		{
			uint8_t ToPort;
			uint8_t Length;
		};
#pragma pack(pop)
		void PortForward(AsioHeader Header);
		template <typename... TArgument>
		uint8_t SyncInvoke(uint8_t RemotePort, TArgument... Arguments)
		{
			uint8_t const LocalPort = AllocatePort();
			{
				SendSession const Session{sizeof(uint8_t) + _TypesSize<TArgument...>::value, RemotePort, BaseStream};
				BaseStream.write(LocalPort);
				size_t const Written[] = {BaseStream.write(reinterpret_cast<char *>(&Arguments), sizeof(Arguments))...};
				// 必须在此处删除SendSession以启用中断，因为下面要同步监听
			}
			uint8_t const MessageSize = Listen(LocalPort);
			ReleasePort(LocalPort);
			return MessageSize;
		}

	public:
		// 从基础流读出平凡对象。只有在Listen方法允许的“手动从基础流读出”语境中才能使用此方法。
		template <typename T>
		AsyncStream const &operator>>(T &Value) const
		{
			BaseStream.readBytes(reinterpret_cast<char *>(&Value), sizeof(T));
			return *this;
		}
		// 从基础流读出平凡对象。只有在Listen方法允许的“手动从基础流读出”语境中才能使用此方法。
		template <typename T>
		T Read() const
		{
			T Value;
			BaseStream.readBytes(reinterpret_cast<char *>(&Value), sizeof(T));
			return Value;
		}
		// 从基础流跳过指定长度的字节。只有在Listen方法允许的“手动从基础流读出”语境中才能使用此方法。
		void Skip(uint8_t Length) const;

		Stream &BaseStream;
		/*
				构造后，BaseStream将交由AsyncStream包装接管。不应再对BaseStream直接进行以下操作，否则行为未定义：
					setTimeout，不支持此方法
					write，应改用Send方法，或使用SendSession。
					readBytes，应改用Listen方法，仅在提供给Listen的回调中允许使用readBytes。
				*/
		AsyncStream(Stream &BaseStream = Serial)
			: BaseStream(BaseStream)
		{
			BaseStream.setTimeout(-1);
		}

		/*
				一般应在loop中调用此方法。它将实际执行所有排队中的监听器。调用此方法前，中断必须处于启用状态。这是因为串口操作需要中断支持，因此必须启用中断。用户在本库其它方法中提供的回调，将被此方法实际调用。所有被委托的Stream在此方法中实际被读。那些Stream的所有操作都应当托管给此方法，用户不应再直接访问那些Stream，否则行为未定义。
				*/
		void ExecuteTransactionsInQueue();

		// 向远程ToPort发送指定Length的Message
		void Send(const void *Message, uint8_t Length, uint8_t ToPort) const;
		// 向远程ToPort发送指定Message。这个Message对象应当允许直接序列化。
		template <typename T>
		inline void Send(const T &Message, uint8_t ToPort) const
		{
			Send(&Message, sizeof(T), ToPort);
		}

		// 分配一个空闲端口号。该端口号将保持被占用状态，不再参与自动分配，直到调用ReleasePort。此方法可能会分配到正在被同步监听或调用的端口，因为它们不视为对端口的占用。
		uint8_t AllocatePort();
		// 检查指定端口Port是否被占用。此方法不能检测Port是否正被同步监听或调用，因为它们不视为对端口的占用。
		bool PortOccupied(uint8_t Port) const
		{
			InterruptGuard const _;
			return _Listeners.contains(Port);
		}
		// 立即释放指定本地端口，取消任何异步监听或绑定函数。不能取消同步监听或调用的，因为它们不视为对端口的占用。返回true表示成功释放了端口，false表示端口原本就未被占用。
		bool ReleasePort(uint8_t Port)
		{
			InterruptGuard const _;
			return _Listeners.erase(Port);
		}

		/*
				异步监听本地FromPort端口。当远程传来指向FromPort的消息时，调用 void Callback(uint8_t MessageSize)，由用户负责手动从基础流读出消息内容。在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
				如果FromPort已被监听，将覆盖。
				此监听是持续性的，每次收到消息都会重复调用Callback。如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort，也可以在那之后安全重用本端口。此方法保证Callback返回前不会被释放。
				此方法保证Callback被调用时中断处于启用状态。
				*/
		template <typename T>
		void Listen(T &&Callback, uint8_t FromPort)
		{
			InterruptGuard const _;
			_Listeners[FromPort] = std::forward<T>(Callback);
		}
		/*
				自动分配一个空闲端口并异步监听。当远程传来指向该端口的消息时，调用 void Callback(uint8_t MessageSize)，由用户负责手动从基础流读出消息内容。在Callback返回之前，必须不多不少恰好读入全部MessageSize字节，否则行为未定义。返回分配的端口号。
				此监听是持续性的，每次收到消息都会重复调用Callback。如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort。
				此方法保证Callback被调用时中断处于启用状态。
				Callback可以安全释放本端口，也可以在那之后安全重用本端口。此方法保证Callback返回前不会被释放。
				*/
		template <typename T>
		uint8_t Listen(T &&Callback)
		{
			InterruptGuard const _;
			uint8_t const FromPort = _AllocatePort();
			_Listeners[FromPort] = std::forward<T>(Callback);
			return FromPort;
		}
		/*
				同步监听一个本地端口。此重载必须在可中断语境中调用，否则可能永不返回。调用后将无限等待指定端口，直到其收到消息才会返回此消息的字节数。那之后，用户必须手动从基础流读出那个数目的字节。
				此监听是同步的，优先于所有异步监听和调用。可以使用此方法监听已被异步监听或调用的端口，也可以在此方法等待期间发生的中断中对同一个端口添加异步监听和调用。在这些情境下，那个异步监听或调用不会比此同步监听更早收到消息，因为异步监听和调用实际上发生在ExecuteTransactionsInQueue中。
				同步监听不占用端口。不能用ReleasePort取消同步监听，且同步监听中的端口可能被AllocatePort分配。
				谨慎使用此重载。如果中断被禁止，此方法将永不返回。如果指定端口一直收不到消息，此方法也将永不返回。
				*/
		uint8_t Listen(uint8_t FromPort);

		/*
				将任意可调用对象绑定到指定本地端口上作为服务，当收到消息时调用。如果端口被占用，将覆盖。
				远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Argument_message_incomplete。
				Function的参数和返回值（如果有）必须是平凡类型。
				此方法保证Function被调用时中断处于启用状态。
				使用ReleasePort可以取消绑定，释放端口。
				*/
		template <typename T>
		void BindFunctionToPort(T &&Function, uint8_t Port)
		{
			Listen(FunctionListener<T>(std::forward<T>(Function), *this), Port);
		}

		/*
				将任意可调用对象绑定到自动分配的空闲端口上作为服务，当收到消息时调用。返回分配的端口号。
				远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8_t的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。如果消息字节数不足，将向远程调用方发送Argument_message_incomplete异常。
				Function的参数和返回值（如果有）必须是平凡类型。
				此方法保证Function被调用时中断处于启用状态。
				使用ReleasePort可以取消绑定，释放端口。
				*/
		template <typename T>
		uint8_t BindFunctionToPort(T &&Function)
		{
			return Listen(FunctionListener<T>(std::forward<T>(Function), *this));
		}

		/*
				异步调用指定RemotePort上的函数，传入任意参数Arguments，所有参数类型必须支持直接序列化。当远程函数返回时，将发送到LocalPort，然后调用 void Callback(Exception Result,ReturnType Return)，其中ReturnType必须与远程函数定义的返回值类型相同，且支持直接反序列化。如果远程函数没有返回值，则Callback必须只接受一个 Exception Result 参数。
				如果远程端口未被监听，Callback将永不会被调用，但LocalPort会被持续占用，直到用户手动ReleasePort。
				如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
				如果Result为Argument_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
				此方法保证Callback被调用时中断处于启用状态。
				如果LocalPort已被占用，将覆盖。Callback被调用前，LocalPort已释放。
				*/
		template <typename TCallback, typename... TArgument>
		std::void_t<std::_FunctionSignature_t<TCallback>> AsyncInvoke(uint8_t RemotePort, uint8_t LocalPort, TCallback &&Callback, TArgument... Arguments)
		{
			Listen(CallbackListener<TCallback>(std::forward<TCallback>(Callback), *this, LocalPort), LocalPort);
			SendSession const Session{sizeof(uint8_t) + _TypesSize<TArgument...>::value, RemotePort, BaseStream};
			BaseStream.write(LocalPort);
			size_t const Written[] = {BaseStream.write(reinterpret_cast<char *>(&Arguments), sizeof(Arguments))...};
		}
		/*
				异步调用指定RemotePort上的函数，传入任意参数Arguments，所有参数类型必须支持直接序列化。当远程函数返回时，将发送到自动分配的本地端口，然后调用 void Callback(Exception Result,ReturnType Return)，其中ReturnType必须与远程函数定义的返回值类型相同，且支持直接反序列化。如果远程函数没有返回值，则Callback必须只接受一个 Exception Result 参数。
				如果远程端口未被监听，Callback将永不会被调用，但本地端口会被持续占用，直到用户手动ReleasePort。
				如果Result为Corrupted_object_received，说明收到了意外的返回值。这可能是因为远程函数返回值的类型与预期不符，或者有其它远程对象向本地端口发送了垃圾信息。此次远程调用将被废弃。
				如果Result为Argument_message_incomplete，说明远程函数接收到的参数不完整。这可能是因为远程函数的参数类型与预期不符，或者有其它本地对象向远程端口发送了垃圾信息。此次远程调用将被废弃。
				此方法保证Callback被调用时中断处于启用状态。
				此函数返回自动分配的本地端口，用户可以用ReleasePort释放此端口，将放弃等待远程函数的返回结果。Callback被调用前，会自动释放该本地端口。
				*/
		template <typename TCallback, typename... TArgument>
		auto AsyncInvoke(uint8_t RemotePort, TCallback &&Callback, TArgument... Arguments)->decltype(static_cast<uint8_t>((Callback(),1)))
		{
			uint8_t const LocalPort = Listen(CallbackListener<TCallback>(std::forward<TCallback>(Callback), *this, LocalPort));
			SendSession const Session{sizeof(uint8_t) + _TypesSize<TArgument...>::value, RemotePort, BaseStream};
			BaseStream.write(LocalPort);
			size_t const Written[] = {BaseStream.write(reinterpret_cast<char *>(&Arguments), sizeof(Arguments))...};
			return LocalPort;
		}
		// 异步调用指定RemotePort上的函数，传入任意参数Arguments，所有参数类型必须支持直接序列化。此重载不期待远程返回值，所以也不分配或占用本地端口。如果远程服务调用发生异常，也不会收到任何反馈。
		template <typename... TArgument>
		std::enable_if_t<std::conjunction<std::is_trivial<TArgument>...>::value> AsyncInvoke(uint8_t RemotePort, TArgument... Arguments)
		{
			SendSession const Session{sizeof(uint8_t) + _TypesSize<TArgument...>::value, RemotePort, BaseStream};
			BaseStream.write(255); // 使用无效端口号255，表示不期待返回值
			size_t const Written[] = {BaseStream.write(reinterpret_cast<char *>(&Arguments), sizeof(Arguments))...};
		}
		/*
				同步调用指定RemotePort上的函数，传入任意参数Arguments，所有参数类型必须支持直接序列化。尽管不期待返回值，此方法仍将内部分配一个空闲临时本地端口，然后无限等待，直到收到远程反馈成功或异常。
				调用此方法前，中断必须处于启用状态。同步调用不占用端口，不能用ReleasePort取消同步调用。
				谨慎使用此方法。如果中断被禁止，此方法将永不返回。如果远程一直不返回，此方法也将永不返回。
				*/
		template <typename... TArgument>
		Exception SyncInvokeWithoutReturn(uint8_t RemotePort, TArgument... Arguments)
		{
			return SyncInvoke(RemotePort, Arguments...) == sizeof(Exception) ? Read<Exception>() : Exception::Corrupted_object_received;
		}
		/*
				同步调用指定RemotePort上的函数，传入接受返回值的引用，以及任意参数Arguments，所有参数类型必须支持直接序列化。此方法将内部分配一个临时本地端口用于接收远程返回值，然后无限等待，直到收到远程返回消息，设置引用值，然后返回成功或异常。
				调用此方法前，中断必须处于启用状态。同步调用不占用端口，不能用ReleasePort取消同步调用。
				谨慎使用此方法。如果中断被禁止，此方法将永不返回。如果远程一直不返回，此方法也将永不返回。
				*/
		template <typename TReturn, typename... TArgument>
		Exception SyncInvokeWithReturn(uint8_t RemotePort, TReturn &ReturnValue, TArgument... Arguments)
		{
			switch (uint8_t const MessageSize = SyncInvoke(RemotePort, Arguments...))
			{
			case sizeof(Exception):
				return Read<Exception>();
			case sizeof(Exception) + sizeof(TReturn):
			{
				Exception const E = Read<Exception>();
				*this >> ReturnValue;
				return E;
			}
			default:
				Skip(MessageSize);
				return Exception::Corrupted_object_received;
			}
		}
	};
}