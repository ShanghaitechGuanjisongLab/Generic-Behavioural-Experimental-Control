classdef AsyncSerialStream<handle
	properties(SetAccess=immutable,GetAccess=protected)
		Serial
		Listeners=dictionary;
	end
	properties(Constant,Access=protected)
		MagicByte=0x5A
	end
	methods(Access=protected)
		function PortForward(obj,varargin)
			while(obj.Serial.NumBytesAvailable)
				if obj.Serial.read(1,'uint8')==Gbec.AsyncSerialStream.MagicByte
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
		function Port=GetFreePort(obj)
			%获取一个空闲的端口号
			Port=0;
			while obj.Listeners.isKey(Port)
				Port=Port+1;
				if Port>255
					Gbec.Exception.AsyncSerialStream_port_exhausted.Throw;
				end
			end
		end
		function FunctionCallback(obj,Function,Arguments)
			%TODO：处理异常情况
			obj.Send(Function(Arguments(2:end)),Arguments(1));
		end
	end
	methods
		function obj = AsyncSerialStream(Port,BaudRate)
			%输入端口号和波特率（可选，默认9600），创建一个异步串口流对象
			arguments
				Port
				BaudRate=9600
			end
			obj.Serial=serialport(Port,BaudRate);
		end
		function Send(obj,Message,ToPort)
			%向远程ToPort发送指定Message。Message必须支持typecast为uint8类型。
			obj.Serial.write(Gbec.AsyncSerialStream.MagicByte,'uint8');
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
				FromPort=obj.GetFreePort
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
				FromPort=obj.GetFreePort
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
				Gbec.Exception.AsyncSerialStream_port_not_found.Warn(Port);
			end
		end
		function Port=BindFunctionToPort(obj,Function,Port)
			%将Function绑定到Port（如果不指定则自动分配空闲端口并返回）作为服务，当收到消息时调用。Function必须接受一个(1,:)uint8输入。远程要调用此对象，也需要将所有参数序列化成一个(1,:)uint8，并且头部额外加上一个uint8的远程端口号用来接收返回值，然后将此消息发送到此函数绑定的本地端口。Function返回值必须支持typecast为uint8类型。
			arguments
				obj
				Function
				Port=obj.GetFreePort
			end
			obj.Listen(@(Arguments)obj.FunctionCallback(Function,Arguments),Port);
		end
	end
end