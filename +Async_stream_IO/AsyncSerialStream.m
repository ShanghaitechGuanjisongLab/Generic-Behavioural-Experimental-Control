classdef AsyncSerialStream<Async_stream_IO.IAsyncStream
	%端口号范围0~254，255是无效端口
	properties(SetAccess=immutable,Transient)
		Serial
	end
	properties(SetAccess=immutable,GetAccess=protected)
		Listeners
	end
	methods(Access=protected)
		function Data=ReadBytes(obj,NumBytes)
			Data=obj.Serial.read(NumBytes,'uint8');
			NumRead=numel(Data);

			%进入这个循环的概率很小，因此优先保证首次读取的性能
			while NumRead<NumBytes
				NewData=obj.Serial.read(NumBytes-NumRead,'uint8');
				NumNew=numel(NewData);

				%虽然每个回合扩张数组通常是不好的，但每个循环发生的概率指数递减，优先保证本次循环的性能为要
				Data(NumRead+1:NumRead+NumNew)=NewData;

				NumRead=NumRead+NumNew;
			end
		end
		function Byte=ReadByte(obj)
			Byte=obj.Serial.read(1,'uint8');

			%进入循环的概率很小，优先保证首次读取性能
			while isempty(Byte)
				Byte=obj.Serial.read(1,'uint8');
			end
		end
		function PortForward(obj,Port,MessageSize)
			%将消息转发到指定端口的监听器
			if obj.Listeners.isKey(Port)
				Listener=obj.Listeners(Port);
				Listener(MessageSize);
			else
				Async_stream_IO.Exception.Unlistened_port_received_message.Warn(sprintf('Port %u, MessageSize %u',Port,MessageSize));
			end
		end
		function FunctionListener(obj,Function,MessageSize)
			if~MessageSize
				return;
			end
			Port=obj.ReadByte;
			Arguments=obj.ReadBytes(MessageSize-1);
			if Port==255
				Function(Arguments);
				return; %不需要返回值
			end
			try
				Return=Function(Arguments);
				Return=[uint8(Async_stream_IO.Exception.Success);typecast(Return(:),'uint8')];
			catch ME
				%如果函数抛出异常，返回Function_runtime_error异常
				Return=uint8(Async_stream_IO.Exception.Function_runtime_error);
				warning(ME.identifier,'%s',ME.message);
			end
			obj.Send(Return,Port);
		end
		function P=AllocatePort_(obj)
			%仅限内部使用。不会添加到监听器，也不需要释放
			persistent Port;
			if isempty(Port)
				Port=0;
			end
			while obj.Listeners.isKey(Port)
				Port=mod(Port+1,255);
			end
			P=Port;
		end
		function CallbackListener(obj,Callback,LocalPort,MessageSize)
			obj.ReleasePort(LocalPort);
			switch MessageSize
				case 0
					Callback(Async_stream_IO.Exception.Corrupted_object_received);
				case 1
					Callback(Async_stream_IO.Exception(obj.ReadByte));
				otherwise
					Exception=Async_stream_IO.Exception(obj.ReadByte);
					if Exception==Async_stream_IO.Exception.Success
						%如果是成功的，读取返回值并调用回调函数
						Callback(obj.ReadBytes(MessageSize-1));
					else
						%否则抛出异常
						obj.ReadBytes(MessageSize-1); %读取剩余字节以避免阻塞
						Callback(Exception);
					end
			end
		end
		function AllocateListener(obj,Port,MessageSize)
			obj.ReadBytes(MessageSize); %读取消息内容以避免阻塞
			Async_stream_IO.Exception.Message_received_on_allocated_port.Warn(sprintf('Port %u, MessageSize %u',Port,MessageSize));
		end
		function ExecuteTransactionsInQueue(obj,varargin)
			while obj.Serial.NumBytesAvailable
				if obj.ReadByte==Async_stream_IO.IAsyncStream.MagicByte
					%读取端口号
					GetPort=obj.ReadByte;
					obj.PortForward(GetPort,obj.ReadByte);
				end
			end
		end
	end
	methods
		function obj = AsyncSerialStream(Port,BaudRate)
			%输入串口号和波特率（可选，默认9600），创建一个异步串口流对象。如果串口被占用，可选强抢，需要提权。
			arguments
				Port
				BaudRate=9600
			end
			try
				obj.Serial=serialport(Port,BaudRate);
			catch ME
				if ME.identifier=="serialport:serialport:ConnectionFailed"&&ismember(Port,serialportlist)&&questdlg('指定串口被其它进程占用，是否抢夺？','串口被占用','抢夺','放弃','放弃')=="抢夺"
					obj.Serial=MATLAB.SnatchSerialport(Port,BaudRate);
				else
					ME.rethrow;
				end
			end
			obj.Listeners=dictionary;
		end
		function P=AllocatePort(obj)
			%获取一个空闲的端口号。直到ReleasePort之前该端口不会被自动分配给其它用途。
			P=obj.AllocatePort_;
			obj.Listeners(P)=@(MessageSize)obj.AllocateListener(P,MessageSize);
			obj.Serial.configureCallback('byte',1,@obj.ExecuteTransactionsInQueue);
		end
		function LocalPort=AsyncInvoke(obj,RemotePort,varargin)
			%异步用指定远程端口上的函数。
			%# 语法
			% ```
			% obj.AsyncInvoke(RemotePort,Argument1,Argument2,…);
			% %此语法不期待远程返回值，所以也不分配或占用本地端口。如果远程调用异常，也不会收到任何反馈。
			%
			% LocalPort=obj.AsyncInvoke(RemotePort,Callback,Argument1,Argument2,…);
			% %此语法期待远程返回值。分配一个本地端口来监听远程返回值，远程返回时提供的Callback将被调用。
			%
			% obj.AsyncInvoke(RemotePort,LocalPort,Callback,Argument1,Argument2,…);
			% %手动指定要监听的本地端口LocalPort，用于接收远程返回值。远程返回时提供的Callback将被调用。
			% ```
			%# 输入参数
			% RemotePort(1,1)，远程服务的端口号
			% Callback function_handle，当远程返回值时调用。如果远程未返回值，Callback将永不会被调用。如果远程返回异常，将调用Callback并提供一个Exception对象作为参
			%  数。如果远程成功返回，将调用Callback并提供一个(:,1)uint8的返回值作为参数。
			% LocalPort(1,1)，要监听的本地端口。如果端口已被占用，将覆盖。使用ReleasePort以放弃接收返回值。远程返回后，无论操作成功还是失败，会自动释放端口。
			% Argument1,Argument2,…，要传递给远程函数的参数。每个参数都必须能typecast为uint8。
			%# 返回值
			% LocalPort(1,1)，自动分配的空闲端口号。使用ReleasePort以放弃接收返回值。远程返回后，无论操作成功还是失败，会自动释放端口。
			NumVarArgs=numel(varargin);
			if NumVarArgs>=1&&isa(varargin{1},'function_handle')
				LocalPort=obj.AllocatePort_;
				Callback=varargin{1};
				varargin(1)=[];
			elseif NumVarArgs>=2&&isa(varargin{2},'function_handle')
				LocalPort=varargin{1};
				Callback=varargin{2};
				varargin(1:2)=[];
			else
				Arguments=cellfun(@(x)typecast(x(:),'uint8'),varargin,UniformOutput=false);
				obj.Send(vertcat(0xff,Arguments{:}),RemotePort);
				return; %不需要监听返回值
			end
			obj.Listen(@(MessageSize)obj.CallbackListener(Callback,LocalPort,MessageSize),LocalPort);
			Arguments=cellfun(@(x)typecast(x(:),'uint8'),varargin,UniformOutput=false);
			obj.Send(vertcat(LocalPort,Arguments{:}),RemotePort);
		end
		function Port=BindFunctionToPort(obj,Function,Port)
			%将本地服务函数绑定到端口，等待远程调用。
			%将任意可调用对象绑定端口上作为服务，当收到消息时调用。远程要调用此对象，需要将所有参数序列化拼接成一个连续的内存块，并且头部加上一个uint8的远程端口号用来
			%  接收返回值，然后将此消息发送到此函数绑定的本地端口。如果远程不期待返回值，应将端口号设为255。
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
			% Function function_handle((:,1)uint8)，当远程调用时，将调用Function((:,1)uint8)。如果有返回值，只能有一个返回值，且必须能够typecast为(:,1)uint8。如果
			%  Function抛出异常，远程将收到Function_runtime_error，但不会得知异常的细节。如果需要，请自行设计返回值结构以反馈异常细节。
			% Port(1,1)，要绑定的端口。如果端口已被占用，将覆盖。
			%# 返回值
			% Port(1,1)，自动分配的空闲端口号。
			arguments
				obj
				Function
				Port=obj.AllocatePort_
			end
			obj.Listen(@(MessageSize)obj.FunctionListener(Function,MessageSize),Port);
		end
		function Correct=CheckArguments(obj,Port,BaudRate)
			%检查当前流的构造参数是否正确
			Correct=obj.Serial.Port==Port&&obj.Serial.BaudRate==BaudRate;
		end
		function PB=Listen(obj,varargin)
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
			if isa(varargin{1},'function_handle')
				Callback=varargin{1};
				if numel(varargin)>1
					PB=varargin{2};
				else
					PB=obj.AllocatePort_;
				end
				obj.Listeners(PB)=Callback;
				obj.Serial.configureCallback('byte',1,@obj.ExecuteTransactionsInQueue);
			else
				ListeningPort=varargin{1};
				while true
					if obj.ReadByte==Async_stream_IO.IAsyncStream.MagicByte
						%读取端口号
						GetPort=obj.ReadByte;
						MessageSize=obj.ReadByte;
						if GetPort==ListeningPort
							%如果端口号匹配，返回消息字节数
							PB=MessageSize;
							return;
						else
							obj.PortForward(GetPort,MessageSize);
						end
					end
				end
			end
		end
		function O=PortOccupied(obj,Port)
			%检查指定端口Port是否被占用
			O=obj.Listeners.isKey(Port);
		end
		function R=ReleasePort(obj,Port)
			%立即释放指定本地端口，取消任何异步监听或绑定函数。返回true表示成功释放了端口，false表示端口原本就未被占用。
			R=obj.Listeners.isKey(Port);
			if R
				obj.Listeners.remove(Port);
				if~obj.Listeners.NumEntries
					%如果没有监听器了，取消串口的回调配置
					obj.Serial.configureCallback('off');
				end
			end
		end
		function Send(obj,Message,ToPort)
			%向远程ToPort发送指定Message。Message必须支持typecast为uint8类型。Message在串口传输时会加上报文头，所以不能多次Send而指望这些消息会自动串联在一起，单个
			% 报文必须串联后一次性交给Send发送。如果希望多次发送字节拼接成单个报文，请使用SendSession。
			obj.Serial.write(Async_stream_IO.IAsyncStream.MagicByte,'uint8');
			obj.Serial.write(ToPort,'uint8');
			Message=typecast(Message(:),'uint8');
			obj.Serial.write(numel(Message),'uint8');
			obj.Serial.write(Message,'uint8');
		end
		function Return=SyncInvoke(obj,RemotePort,varargin)
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
			LocalPort=obj.AllocatePort_;
			Arguments=cellfun(@(x)typecast(x(:),'uint8'),varargin,UniformOutput=false);
			obj.Send(vertcat(LocalPort,Arguments{:}),RemotePort);
			MessageSize=obj.Listen(LocalPort);
			if MessageSize<1
				Async_stream_IO.Exception.Corrupted_object_received.Throw(sprintf('RemotePort %u, LocalPort %u, MessageSize %u',RemotePort,LocalPort,MessageSize));
			end
			Exception=Async_stream_IO.Exception(obj.ReadByte);
			Return=obj.ReadBytes(MessageSize-1);
			if Exception~=Async_stream_IO.Exception.Success
				Exception.Throw(sprintf('RemotePort %u, LocalPort %u, MessageSize %u',RemotePort,LocalPort,MessageSize));
			end
		end
		function Data=Read(obj,Type,Number)
			%从基础流直接读出数据类型
			% 此方法仅用于配合Listen方法实现同步读入消息，其它情形不应使用此方法，以免破坏数据报文。累计读入字节数不能超过Listen返回的字节数。
			%# 语法
			% ```
			% Data=Read(obj,Type);
			% 读入1个指定类型的数据
			%
			% Data=Read(obj,Type,Number);
			% 读入指定数量的指定类型的数据
			% ```
			%# 输入参数
			% Type(1,1)string，要读取的数据类型，注意不同类型数据有不同的字节数
			% Number(1,1)，要读取的数据数量
			%# 返回值
			% Data(1,Number)Type，读取到的数据。
			%See also Async_stream_IO.IAsyncStream.Listen
			arguments
				obj
				Type
				Number=1
			end
			Data=obj.Serial.read(Number,Type);
			NumRead=numel(Data);

			%进入这个循环的概率很小，因此优先保证首次读取的性能
			while NumRead<Number
				NewData=obj.Serial.read(Number-NumRead,Type);
				NumNew=numel(NewData);

				%虽然每个回合扩张数组通常是不好的，但每个循环发生的概率指数递减，优先保证本次循环的性能为要
				Data(NumRead+1:NumRead+NumNew)=NewData;

				NumRead=NumRead+NumNew;
			end
		end
	end
end