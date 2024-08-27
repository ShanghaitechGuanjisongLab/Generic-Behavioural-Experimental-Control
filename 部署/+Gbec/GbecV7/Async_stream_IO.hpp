#pragma once
#include <Cpp_Standard_Library.h>
#include <dynarray>
#include <functional>
#include <Arduino.h>
namespace Async_stream_IO
{
	enum class Exception
	{
		Success,
		Port_occupied,
		Insufficient_message_capacity,
		Port_idle,
	};

	// API层级0：报文收发

	/*一般应在loop中调用此方法。它将实际执行所有排队中的事务，包括发送消息和调用监听器。此方法执行过程中会允许中断，即使调用前处于不可中断状态。此方法返回之前会恢复调用前的中断状态。
	不应在此方法执行期间存在任何可能会直接调用Stream读写操作的中断，可能破坏数据结构。只能使用此命名空间中的方法进行委托。
	*/
	void ExecuteTransactionsInQueue();

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送指定长度的消息。调用方应确保那时Message指针仍然有效。消息发送后，调用Callback通知委托完成。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	如果在Callback中再次调用Send要非常小心，因为那个委托会直接在本次ExecuteTransactionsInQueue中执行而不等到下一次，然后再次调用Callback，如果那个Callback中又有Send，这样就存在形成无限循环的风险。
	*/
	void Send(const char *Message, uint8_t Length, uint8_t ToPort, std::move_only_function<void() const> &&Callback, Stream &ToStream = Serial);

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送指定长度的消息。调用方应确保那时Message指针仍然有效。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	*/
	void Send(const char *Message, uint8_t Length, uint8_t ToPort, Stream &ToStream = Serial);

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送消息。Message的所有权将被转移到内部，调用方不应再使用它。消息发送后，可选调用Callback通知委托完成。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	如果在Callback中再次调用Send要非常小心，因为那个委托会直接在本次ExecuteTransactionsInQueue中执行而不等到下一次，然后再次调用Callback，如果那个Callback中又有Send，这样就存在形成无限循环的风险。
	*/
	void Send(std::dynarray<char> &&Message, uint8_t ToPort, std::move_only_function<void() const> &&Callback, Stream &ToStream = Serial);

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送消息。Message的所有权将被转移到内部，调用方不应再使用它。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	*/
	void Send(std::dynarray<char> &&Message, uint8_t ToPort, Stream &ToStream = Serial);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity。
	 */
	Exception Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供消息内容Message。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	 */
	Exception Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。在停止监听前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，监听仍然继续。
	 */
	Exception Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，调用Callback，并提供消息内容Message。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	 */
	Exception Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, uint8_t FromPort, Stream &FromStream = Serial);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity。
	 */
	uint8_t Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream = Serial);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，调用Callback，提供消息内容Message。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	 */
	uint8_t Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream = Serial);

	/*自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。在停止监听前，调用方有义务维持Message指针有效。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，监听仍然继续。
	 */
	uint8_t Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream = Serial);

	// 自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，调用Callback，并提供消息内容Message。
	uint8_t Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream = Serial);

	// 释放指定端口，取消任何Receive或Listen监听。如果端口未被监听，返回Port_idle。无论如何，此方法返回后即可以在被释放的端口上附加新的监听。
	Exception ReleasePort(uint8_t Port);

	// API层级1：函数和对象

	// 递归模板终止条件
	template <typename...>
	struct _SizeOfSum;

	// 递归模板：处理一个或多个类型
	template <typename First, typename... Rest>
	struct _SizeOfSum<First, Rest...>
	{
		static constexpr size_t value = sizeof(First) + SizeOfSum<Rest...>::value;
	};

	// 递归模板的基例：空参数包
	template <>
	struct _SizeOfSum<>
	{
		static constexpr size_t value = 0;
	};

#pragma pack(push, 1)
#pragma pack(pop)
template<typename...T>
struct CompactTuple
{

};

	template <typename ReturnType, typename... ArgumentTypes>
	ReturnType InvokeWithStream(ReturnType (*Invoker)(ArgumentTypes...), Stream &ArgumentStream = Serial)
	{
		constexpr size_t ArgumentSize = _SizeOfSum<ArgumentTypes...>::value;
		char ArgumetsCache[ArgumetSize];
		ArgumentStream.readBytes(ArgumetsCache, ArgumentSize);
	}

	// 从流中读取可调用对象的参数并调用之
	template <typename Invokable>
	void InvokeWithStream(Invokable Invoker, Stream &ArgumentStream = Serial)
	{
	}
	// 辅助类型：用于提取函数调用运算符的返回类型和参数类型
	template <typename T>
	struct lambda_traits;

	// 偏特化用于lambda表达式
	template <typename ClassType, typename ReturnType, typename... Args>
	struct lambda_traits<ReturnType (ClassType::*)(Args...) const>
	{
		using return_type = ReturnType;
		using argument_types = std::tuple<Args...>;
	};

	// 提取lambda的返回类型
	template <typename Lambda>
	using lambda_return_type = typename lambda_traits<decltype(&Lambda::operator())>::return_type;

	// 提取lambda的参数类型
	template <typename Lambda>
	using lambda_argument_types = typename lambda_traits<decltype(&Lambda::operator())>::argument_types;

	void Test(Stream &ArgumentStream = Serial)
	{
		int x = 10;
		auto lambda = [x](int a, double b) -> bool
		{
			return a > b + x;
		};

		using ReturnType = lambda_return_type<decltype(lambda)>;
		using ArgumentTypes = lambda_argument_types<decltype(lambda)>;
	}
}