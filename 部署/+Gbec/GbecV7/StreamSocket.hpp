#pragma once
#include <functional>
#include <dynarray>
namespace StreamSocket
{
	enum class Exception
	{
		Success,
		Port_occupied,
		Port_bound,
		Port_unbound,
		Buffer_overflow,
		Stream_timeout,
		TCP_not_connected
	};
	// 不可拷贝类型的通用声明
#define StreamSocket_MoveOnly(Struct)           \
	Struct(const Struct &) = delete;            \
	Struct(Struct &&);                          \
	Struct &operator=(const Struct &) = delete; \
	Struct &operator=(Struct &&);               \
	~Struct();
	// 向不特定的多个目标端口一次性收发消息。此类不能拷贝只能移动。
	struct UdpSocket
	{
		// 使用指定的基础流构造对象。调用方负责设置流的超时时间、波特率等参数。所有Receive重载都可能返回Stream_timeout，表示流读取超时。
		UdpSocket(Stream &Base);
		StreamSocket_MoveOnly(UdpSocket);
		// 绑定到指定端口。如果端口被占用，返回Port_occupied。此函数返回Success后，再次调用将返回Port_bound。所有Stream共享一套端口编号系统。如果没有Bind就调用Receive，将立即返回Port_unbound。析构此对象以释放端口。
		Exception Bind(uint8_t Port);
		// 注册异步接收消息，收到消息时执行Handler函数。Handler函数接收两个参数，第一个是消息内容，第二个是消息来源端口。此方法只能接受一条消息，如果需要连续接收消息，应该在Handler函数中再次调用Receive，或者使用TCP。
		Exception Receive(std::move_only_function<void(std::dynarray<char> &&Buffer, uint8_t From) const> &&Handler) const;
		// 使用指定的Buffer，注册异步接收消息，收到消息时执行Handler函数，Handler函数接受一个参数，表示发送方端口号。此方法只能接受一条消息，如果需要连续接收消息，应该在Handler函数中再次调用Receive，或者使用TCP。
		Exception Receive(std::vector<char> &Buffer, std::move_only_function<void(uint8_t From) const> &&Handler) const;
		// 使用指定的Buffer和Capacity，注册异步接收消息。收到消息时执行Handler函数，Handler函数接受两个参数，第一个是消息长度，第二个是发送方端口号。如果NumBytes>Capacity，说明消息长度超过缓冲区容量，未能容纳的字节将被丢弃。此方法只能接受一条消息，如果需要连续接收消息，应该在Handler函数中再次调用Receive，或者使用TCP。
		Exception Receive(char *Buffer, uint16_t Capacity, std::move_only_function<void(uint16_t NumBytes, uint8_t From) const> &&Handler) const;
		// 向指定端口To发送NumBytes字节数的消息。如果没有绑定端口，默认发送端口号为0，表示匿名消息。
		void Send(const char *Buffer, uint16_t NumBytes, uint8_t To) const;
	};
	// 向单个特定目标端口持续收发消息。可以直接构造对象以请求TCP连接，也可以使用TcpListener等待TCP连接请求。此类不能拷贝只能移动。
	struct TcpSocket
	{
		// 使用指定的基础流，占用This端口构造对象。调用方负责设置流的超时时间、波特率等参数。如果构造成功，Result将被设为Success；如果端口被占用，设为Port_occupied。构造成功后，应调用Connect以请求连接到远程端口。
		TcpSocket(Stream &Base, uint8_t This, Exception &Result);
		// 注册异步接收消息，收到消息时执行Handler函数，Buffer中包含消息内容。如果TCP连接无效，返回TCP_not_connected。
		Exception Receive(std::move_only_function<void(std::dynarray<char> &&Buffer) const> &&Handler) const;
		// 使用指定的Buffer，注册异步接收消息，收到消息时执行Handler函数，Handler函数接受一个参数，表示发送方端口号。
		Exception Receive(std::vector<char> &Buffer, std::move_only_function<void(uint8_t From) const> &&Handler);
		// 使用指定的Buffer和Capacity，注册异步接收消息。收到消息时执行Handler函数，Handler函数接受两个参数，第一个是消息长度，第二个是发送方端口号。如果NumBytes>Capacity，说明消息长度超过缓冲区容量，未能容纳的字节将被丢弃。
		Exception Receive(char *Buffer, uint16_t Capacity, std::move_only_function<void(uint16_t NumBytes, uint8_t From) const> &&Handler);
		StreamSocket_MoveOnly(TcpSocket);

	protected:
		friend struct TcpListener;
		// 仅供TcpListener使用
		TcpSocket(Stream &Base, uint8_t This, uint8_t That);
	};
	// 等待远程端口请求TCP连接。此类不能拷贝只能移动。
	struct TcpListener
	{
		/*监听This端口。收到TCP请求后，将向Listener提供TcpSocket和对方端口That，可以选择保留此Socket用于后续收发报文或者抛弃此Socket以拒绝连接。
		使用指定的基础Stream构造对象。调用方负责设置流的超时时间、波特率等参数。如果构造成功，Result将被设为Success；如果端口被占用，设为Port_occupied。
		析构此对象以停止监听，已经建立的连接仍然有效。
		*/
		TcpListener(Stream &Base, uint8_t This, std::move_only_function<void(TcpSocket &&Socket, uint8_t That) const> &&Listener, Exception &Result);
		StreamSocket_MoveOnly(TcpListener);
	};
	// 实际处理所有异步注册的Receive和Listen。调用方应该在主循环中调用此函数。
	void Loop();
}