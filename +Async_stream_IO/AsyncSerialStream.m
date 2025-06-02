classdef AsyncSerialStream<handle
	%端口号范围0~255
	properties(SetAccess=immutable)
		Serial
	end
	properties(SetAccess=immutable,GetAccess=protected)
		Listeners=dictionary;
	end
	properties(Constant,Access=protected)
		MagicByte=0x5A
	end
	methods(Access=protected)
		function PortForward(obj,varargin)
			while(obj.Serial.NumBytesAvailable)
				if obj.Serial.read(1,'uint8')==Async_stream_IO.AsyncSerialStream.MagicByte
					%读取端口号
					Port=obj.Serial.read(1,'uint8');
					%读取消息内容。即使端口号不在监听器列表中，也要读取消息内容以避免阻塞。
					Message=obj.Serial.read(obj.Serial.read(1,'uint8'),'uint8');
					if obj.Listeners.isKey(Port)
						%如果有监听器，调用其回调函数
						Arguments=obj.Listeners(Port);
						if Arguments.Once
							%如果是一次性监听器，删除它
							obj.Listeners.remove(Port);
						end
						Arguments.Callback(Message);
					end
				end
			end
		end
		function P=AllocatePort(obj)
			%获取一个空闲的端口号
			persistent Port;
			if isempty(Port)
				Port=0;
			end
			while obj.Listeners.isKey(Port)
				Port=mod(Port+1,256);
			end
			P=Port;
		end
		function FunctionCallback(obj,Function,Arguments)
			try
				Result=Function(Arguments(2:end));
			catch ME
				obj.Send(Async_stream_IO.Exception.Function_runtime_error,Arguments(1));
				warning(ME.identifier,'%s',ME.message);
				return;
			end
			obj.Send([uint8(Async_stream_IO.Exception.Success), typecast(Result,'uint8')],Arguments(1));
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
		end
		function Send(obj,Message,ToPort)
			%向远程ToPort发送指定Message。Message必须支持typecast为uint8类型。Message在串口传输时会加上报文头，所以不能多次Send而指望这些消息会自动串联在一起。
			obj.Serial.write(Async_stream_IO.AsyncSerialStream.MagicByte,'uint8');
			obj.Serial.write(ToPort,'uint8');
			Message=typecast(Message,'uint8');
			obj.Serial.write(numel(Message),'uint8');
			obj.Serial.write(Message,'uint8');
		end
		function FromPort=Receive(obj,Callback,FromPort)
			%异步接收来自FromPort的消息，传递给Callback函数句柄。Callback必须接受一个(1,:)uint8向量输入。可以不指定FromPort，将自动分配一个空闲端口并返回。收到一次消
			% 息后将删除监听器，如果要持续监听请使用Listen方法。
			arguments
				obj
				Callback
				FromPort=obj.AllocatePort
			end
			obj.Listeners(FromPort)=struct(Once=true,Callback=Callback);
			obj.Serial.configureCallback('byte',1,@obj.PortForward);
		end
		function FromPort=Listen(obj,Callback,FromPort)
			%持续监听来自FromPort的消息，传递给Callback函数句柄。Callback必须接受一个(1,:)uint8输入。可以不指定FromPort，将自动分配一个空闲端口并返回。收到一次消
			% 息后将保留监听器，如果只想监听一次请使用Receive方法。
			arguments
				obj
				Callback
				FromPort=obj.AllocatePort
			end
			obj.Listeners(FromPort)=struct(Once=false,Callback=Callback);
			obj.Serial.configureCallback('byte',1,@obj.PortForward);
		end
		function ReleasePort(obj,Port)
			%释放指定的端口号Port，删除对应的监听器。
			if obj.Listeners.isKey(Port)
				obj.Listeners.remove(Port);
				if isempty(obj.Listeners)
					%如果没有监听器了，取消回调配置
					obj.Serial.configureCallback('off');
				end
			else
				Async_stream_IO.Exception.AsyncSerialStream_port_not_found.Warn(Port);
			end
		end
		function Port=BindFunctionToPort(obj,Function,Port)
			%将Function绑定到Port（如果不指定则自动分配空闲端口并返回）作为服务，当收到消息时调用。Function必须接受一个(1,:)uint8输入。远程要调用此对象，也需要将所有
			% 参数序列化成一个(1,:)uint8，并且头部额外加上一个uint8的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。Function返回值必须支持typecast
			% 为uint8类型。
			arguments
				obj
				Function
				Port=obj.AllocatePort
			end
			obj.Listen(@(Arguments)obj.FunctionCallback(Function,Arguments),Port);
		end
		function RemoteInvoke(obj,RemotePort,Callback,varargin)
			%远程调用指定RemotePort上的函数，传入varargin（每个参数都必须能typecast为uint8）。当远程函数返回时，调用Callback，必须接受一个(1,:)uint8参数。如果远程端
			% 口未被监听，Callback将不会被调用。
			Arguments=cellfun(@(x)typecast(x,'uint8'),varargin);
			obj.Send([uint8(obj.Receive(@(Message)InvokeCallback(Callback,Message))),Arguments{:}],RemotePort);
		end
	end
end
function InvokeCallback(Callback,Message)
if Message(1)==Async_stream_IO.Exception.Success
	%如果消息是成功的，调用回调函数
	Callback(Message(2:end));
else
	%否则抛出异常
	Async_stream_IO.Exception(Message(1)).Throw;
end
end