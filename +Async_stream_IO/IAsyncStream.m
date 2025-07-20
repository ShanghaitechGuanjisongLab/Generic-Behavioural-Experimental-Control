classdef IAsyncStream<handle
	%异步流标准接口，允许开发者自行实现
	methods(Abstract)
		%向远程ToPort发送指定Message。Message必须支持typecast为uint8类型。Message在串口传输时会加上报文头，所以不能多次Send而指望这些消息会自动串联在一起。
		Send(obj,Message,ToPort)

		%异步接收来自FromPort的消息，传递给Callback函数句柄。Callback必须接受一个(1,:)uint8向量输入。可以不指定FromPort，将自动分配一个空闲端口并返回。收到一次消
		% 息后将删除监听器，如果要持续监听请使用Listen方法。
		FromPort=Receive(obj,Callback,FromPort)

		%持续监听来自FromPort的消息，传递给Callback函数句柄。Callback必须接受一个(1,:)uint8输入。可以不指定FromPort，将自动分配一个空闲端口并返回。收到一次消
		% 息后将保留监听器，如果只想监听一次请使用Receive方法。
		FromPort=Listen(obj,Callback,FromPort)

		%释放指定的端口号Port，删除对应的监听器。
		ReleasePort(obj,Port)

		%将Function绑定到Port（如果不指定则自动分配空闲端口并返回）作为服务，当收到消息时调用。Function必须接受一个(1,:)uint8输入。远程要调用此对象，也需要将所有
		% 参数序列化成一个(1,:)uint8，并且头部额外加上一个uint8的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。Function返回值必须支持typecast
		% 为uint8类型。
		Port=BindFunctionToPort(obj,Function,Port)

		%远程调用指定RemotePort上的函数，传入varargin（每个参数都必须能typecast为uint8）。当远程函数返回时，调用Callback，必须接受一个(1,:)uint8参数。如果远程端
		% 口未被监听，Callback将不会被调用。
		%返回监听返回值的端口，该端口为自动分配。使用ReleasePort以放弃接收此返回值。
		LocalPort=AsyncInvoke(obj,RemotePort,Callback,varargin)

		%检查当前流的构造参数是否正确
		Correct=CheckArguments(obj,varargin)

		%获取一个空闲的端口号。直到ReleasePort之前该端口不会被自动分配给其它用途。
		P=AllocatePort(obj)			
	end
end