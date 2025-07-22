classdef IAsyncStream<handle
	%异步流标准接口，允许开发者自行实现
	methods(Abstract)
		%分配一个空闲端口号。该端口号将保持被占用状态，不再参与自动分配，直到调用ReleasePort。
		P=AllocatePort(obj)

		%将本地服务函数绑定到端口，等待远程调用。
		%# 语法
		% ```
		% Port=obj.BindFunctionToPort(Function);
		% %将任意可调用对象绑定到自动分配的空闲端口上作为服务，当收到消息时调用。返回分配的端口号。
		% %远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。
		% %Function必须接受一个(:,1)uint8输入。返回值必须是[logical,(:,1)uint8
		此方法保证Function被调用时中断处于启用状态。
		使用ReleasePort可以取消绑定，释放端口。
		Port=BindFunctionToPort(obj,Function,Port)

		%监听本地端口，等待远程发来消息。
		%# 语法
		% ```
		% Port=obj.Listen(Callback);
		% %自动分配一个空闲端口并异步监听。返回分配的端口号。此监听是持续性的，每次收到消息都会重复调用Callback。
		% %如果需要停止监听，可以在Callback中或其它任何位置调用ReleasePort，也可以在那之后安全重用本端口。
		%
		% obj.Listen(Callback,Port);
		% %手动指定要监听的端口。如果FromPort已被监听，将覆盖。
		%
		% NumBytes=obj.Listen(Port);
		% %同步监听一个本地端口。调用后将无限等待指定端口，直到其收到消息才会返回此消息的字节数。那之后，用户必须在接下来的代码中立即从基础流读出那个数目的字节。
		% %此监听是同步的，优先于所有异步监听和调用。可以使用此方法监听已被异步监听或调用的端口而不覆盖之前的监听和调用，也可以在此方法等待期间发生的中断中对同一个端口添加异
		% % 步监听和调用。在这些情境下，那个异步监听或调用不会比此同步监听更早收到消息。用户在此方法返回并读完返回的字节数之前不应再次调用此方法、SyncInvoke或结束本次代码执
		% % 行会话，否则行为未定义。
		% %谨慎使用此语法。如果指定端口一直收不到消息，此方法将永不返回。
		% ```
		%# 输入参数
		% Callback function_handle(MessageSize(1,1))，当远程传来指向该端口的消息时，将调用Callback(MessageSize)，由用户负责手动从基础流读出消息内容。在Callback返回之前，
		%  必须不多不少恰好读入全部MessageSize字节，否则行为未定义。
		% Port(1,1)，手动指定要监听的端口。
		%# 返回值
		% Port(1,1)，自动分配的空闲端口号。
		% NumBytes(1,1)，同步监听收到的消息字节数。
		Port=Listen(obj,varargin)

		%检查指定端口Port是否被占用。此方法不能检测Port是否正被同步监听或调用，因为它们不视为对端口的占用。
		O=PortOccupied(obj,Port)

		%立即释放指定本地端口，取消任何异步监听或绑定函数。不能取消同步监听或调用的，因为它们不视为对端口的占用。
		ReleasePort(obj,Port)

		%向远程ToPort发送指定Message。Message必须支持typecast为uint8类型。Message在串口传输时会加上报文头，所以不能多次Send而指望这些消息会自动串联在一起。
		Send(obj,Message,ToPort)

		%远程调用指定RemotePort上的函数，传入varargin（每个参数都必须能typecast为uint8）。当远程函数返回时，调用Callback，必须接受一个(1,:)uint8参数。如果远程端
		% 口未被监听，Callback将不会被调用。
		%返回监听返回值的端口，该端口为自动分配。使用ReleasePort以放弃接收此返回值。
		LocalPort=AsyncInvoke(obj,RemotePort,Callback,varargin)

		%检查当前流的构造参数是否正确
		Correct=CheckArguments(obj,varargin)
	end
end