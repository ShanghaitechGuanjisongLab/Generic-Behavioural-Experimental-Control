#pragma once
#include <dynarray>
#include <functional>
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

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送指定长度的消息。调用方应确保那时Message指针仍然有效。消息发送后，可选调用Callback通知委托完成。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	如果在Callback中再次调用Send要非常小心，因为那个委托会直接在本次ExecuteTransactionsInQueue中执行而不等到下一次，然后再次调用Callback，如果那个Callback中又有Send，这样就存在形成无限循环的风险。
	*/
	void Send(const char *Message, uint8_t Length, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback = []() {});

	/* 委托下次ExecuteTransactionsInQueue时，向指定目标发送消息。Message的所有权将被转移到内部，调用方不应再使用它。消息发送后，可选调用Callback通知委托完成。
	写串口操作依赖中断，在不允许中断的状态下使用Stream原生方法写出可能会永不返回，因此需要调用此方法委托延迟写出。反之则可以直接调用Stream提供的写出方法，无需委托。
	如果在Callback中再次调用Send要非常小心，因为那个委托会直接在本次ExecuteTransactionsInQueue中执行而不等到下一次，然后再次调用Callback，如果那个Callback中又有Send，这样就存在形成无限循环的风险。
	*/
	void Send(std::dynarray<char> &&Message, Stream &ToStream, uint8_t ToPort, std::move_only_function<void() const> &&Callback = []() {});

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity。
	 */
	Exception Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort);

	/*监听FromStream流的FromPort端口。当远程传来指向FromPort的消息时，调用Callback，提供消息内容Message。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	 */
	Exception Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。在停止监听前，调用方有义务维持Message指针有效。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，监听仍然继续。
	 */
	Exception Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream, uint8_t FromPort);

	/*持续监听FromStream流的FromPort端口。每当远程传来指向FromPort的消息时，调用Callback，并提供消息内容Message。
	如果FromPort已被监听，那么新的监听将失败，返回Port_occupied。
	 */
	Exception Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream, uint8_t FromPort);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，那个消息将被拷贝到Message指针最多Capacity字节，并调用Callback。在那之前，调用方有义务维持Message指针有效。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity。
	 */
	uint8_t Receive(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream);

	/*自动分配一个空闲端口返回，并监听FromStream流的那个端口。当远程传来指向该端口的消息时，调用Callback，提供消息内容Message。
	此监听是一次性的。一旦收到消息，监听就结束，端口被释放。这意味着，可以在Callback中再次监听同一个端口。
	 */
	uint8_t Receive(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream);

	/*自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，那个消息都将被拷贝到Message指针最多Capacity字节，并调用Callback。在停止监听前，调用方有义务维持Message指针有效。
	如果消息长度超过Capacity，将不会向Message写入任何内容，但仍会调用Callback，提供异常Insufficient_message_capacity，监听仍然继续。
	 */
	uint8_t Listen(char *Message, uint8_t Capacity, std::move_only_function<void(Exception Result) const> &&Callback, Stream &FromStream);

	// 自动分配一个空闲端口返回，并持续监听FromStream流的那个端口。每当远程传来指向该端口的消息时，调用Callback，并提供消息内容Message。
	uint8_t Listen(std::move_only_function<void(std::dynarray<char> &&Message) const> &&Callback, Stream &FromStream);

	// 释放指定端口，取消任何Receive或Listen监听。如果端口未被监听，返回Port_idle。无论如何，此方法返回后即可以在被释放的端口上附加新的监听。
	Exception ReleasePort(uint8_t Port);

	// API层级1：函数和对象

	template <typename ReturnType, typename... ArgumentTypes>
	struct Caller
	{
		static void Call(std::function<ReturnType(ArgumentTypes...)>);
	};
	template <typename ReturnType, typename...ArgumentTypes>
	void Call(std::function<ReturnType(ArgumentTypes...)>);
	void Call()
	{
		Call<void,int>([](int) {});
	}
}