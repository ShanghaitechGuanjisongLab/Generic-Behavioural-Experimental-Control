classdef IAsyncStream<handle
	%异步流标准接口，允许开发者自行实现。可能的端口号为0~254，255保留为无效端口号不会被分配，也不应使用。
	properties(Constant)
		MagicByte=0x5A
	end
	methods(Abstract)
		%分配一个空闲端口号。该端口号将保持被占用状态，不再参与自动分配，直到调用ReleasePort
		P=AllocatePort(obj)

		%异步用指定远程端口上的函数。
		%# 语法
		% ```
		% obj.AsyncInvoke(RemotePort,Argument1,Argument2,…);
		% %此语法不期待远程返回值，所以也不分配或占用本地端口。
		%
		% LocalPort=obj.AsyncInvoke(RemotePort,Callback,Argument1,Argument2,…);
		% %此语法期待远程返回值。分配一个本地端口来监听远程返回值，远程返回时提供的Callback将被调用。
		%
		% obj.AsyncInvoke(RemotePort,LocalPort,Callback,Argument1,Argument2,…);
		% %手动指定要监听的本地端口LocalPort，用于接收远程返回值。远程返回时提供的Callback将被调用。
		% ```
		%# 输入参数
		% RemotePort(1,1)，远程服务的端口号
		% Callback function_handle，当远程返回值时调用。如果远程未返回值，Callback将永不会被调用。如果远程返回异常，将调用Callback并提供一个Exception对象作为参数。如果远
		%  程成功返回，将调用Callback并提供一个(:,1)uint8的返回值作为参数。
		% LocalPort(1,1)，要监听的本地端口。如果端口已被占用，将覆盖。使用ReleasePort以放弃接收返回值。远程返回后，无论操作成功还是失败，会自动释放端口。
		% Argument1,Argument2,…，要传递给远程函数的参数。每个参数都必须能typecast为uint8。
		%# 返回值
		% LocalPort(1,1)，自动分配的空闲端口号。使用ReleasePort以放弃接收返回值。远程返回后，无论操作成功还是失败，会自动释放端口。
		LocalPort=AsyncInvoke(obj,RemotePort,varargin)

		%将本地服务函数绑定到端口，等待远程调用。
		%将任意可调用对象绑定端口上作为服务，当收到消息时调用。远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8的远程端口号用来接收返回值，
		% 然后将此消息发送到此函数绑定的本地端口。如果远程不期待返回值，应将端口号设为255。
		%使用ReleasePort可以取消绑定，释放端口。
		%# 语法
		% ```
		% Port=obj.BindFunctionToPort(Function);
		% %自动分配空闲端口并返回。
		%
		% obj.BindFunctionToPort(Function,Port);
		% %手动指定要绑定的端口。
		% ```
		%# 输入参数
		% Function function_handle((:,1)uint8)，当远程调用时，将调用Function((:,1)uint8)。如果有返回值，只能有一个返回值，且必须能够typecast为(:,1)uint8。。如果Function
		%  抛出异常，远程将收到Function_runtime_error，但不会得知异常的细节。如果需要，请自行设计返回值结构以反馈异常细节。
		% Port(1,1)，要绑定的端口。如果端口已被占用，将覆盖。
		%# 返回值
		% Port(1,1)，自动分配的空闲端口号。
		Port=BindFunctionToPort(obj,Function,Port)

		%检查当前流的构造参数是否正确
		Correct=CheckArguments(obj,varargin)

		%监听本地端口，等待远程发来消息。
		%# 语法
		% ```
		% Port=obj.Listen(Callback);
		% %自动分配一个空闲端口并异步监听。返回分配的端口号。此监听是持续性的，每次收到消息都会重复调用Callback。
		% %如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort，也可以在那之后安全重用本端口。
		%
		% obj.Listen(Callback,Port);
		% %手动指定要监听的端口。如果FromPort已被监听，将覆盖。如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort，也可以在那之后安全重用本端口。
		%
		% NumBytes=obj.Listen(Port);
		% %同步监听一个本地端口。调用后将无限等待指定端口，直到其收到消息才会返回此消息的字节数。那之后，用户必须在接下来的代码中立即从基础流读出那个数目的字节（使用Read方法）。
		% %此监听是同步的，优先于所有异步监听和调用。可以使用此方法监听已被异步监听或调用的端口而不覆盖之前的监听和调用，也可以在此方法等待期间发生的中断中对同一个端口添加异
		% % 步监听和调用。在这些情境下，那个异步监听或调用不会比此同步监听更早收到消息。用户在此方法返回并读完返回的字节数之前不应再次调用此方法、SyncInvoke或结束本次代码执
		% % 行会话，否则行为未定义。
		% %谨慎使用此语法。如果指定端口一直收不到消息，此方法将永不返回。
		% ```
		%# 输入参数
		% Callback function_handle((1,1)uint8)，当远程传来指向该端口的消息时，将调用Callback(MessageSize)，由用户负责手动从基础流读出消息内容。在Callback返回之前，
		%  必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
		% Port(1,1)，手动指定要监听的端口。
		%# 返回值
		% Port(1,1)，自动分配的空闲端口号。
		% NumBytes(1,1)，同步监听收到的消息字节数。
		%See also Async_stream_IO.IAsyncStream.Read
		Port=Listen(obj,varargin)

		%检查指定端口Port是否被占用
		O=PortOccupied(obj,Port)

		%立即释放指定本地端口，取消任何异步监听或绑定函数。返回true表示成功释放了端口，false表示端口原本就未被占用。
		R=ReleasePort(obj,Port)

		%向远程ToPort发送指定Message。Message必须支持typecast为uint8类型。Message在串口传输时会加上报文头，所以不能多次Send而指望这些消息会自动串联在一起，单个报文必须串
		% 联后一次性交给Send发送。
		Send(obj,Message,ToPort)

		%同步调用指定远程端口上的函数
		%如果远程不返回，此调用也不会返回。如果远程返回异常，将抛出异常。
		%# 语法
		% ```
		% obj.SyncInvoke(RemotePort,Argument1,Argument2,…);
		% %此语法不期待远程返回值，但会等待远程反馈调用成功还是失败。
		%
		% Return=obj.SyncInvoke(RemotePort,Argument1,Argument2,…);
		% %此语法期待远程返回值
		% ```
		%# 输入参数
		% RemotePort(1,1)，远程服务的端口号
		% Argument1,Argument2,…，要传递给远程函数的参数。每个参数都必须能typecast为uint8。
		%# 返回值
		% Return(:,1)uint8，远程函数的返回值。如果远程函数没有返回值，将返回空数组。
		Return=SyncInvoke(obj,RemotePort,varargin)

		%从基础流直接读出数据类型
		% 此方法仅用于配合Listen方法实现同步读入消息，其它情形不应使用此方法，以免破坏数据报文。累计读入字节数不能超过Listen返回的字节数。
		%# 输入参数
		% Type(1,1)string，要读取的数据类型，注意不同类型数据有不同的字节数
		% Number(1,1)，要读取的数据数量
		%# 返回值
		% Data(Number,1)Type，读取到的数据。
		%See also Async_stream_IO.IAsyncStream.Listen
		Data=Read(obj,Type,Number)
	end
end