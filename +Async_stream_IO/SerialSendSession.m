classdef SerialSendSession<handle
	%轻量化的串口消息发送合并器。此对象构造后，用户必须用Serial.write或<=运算符，在单次执行会话中，可以多次写入，而不能使用AsyncSerialStream.Send；此方法发送的所有数
	% 据会被字节串联并视为单条消息。必须恰好写入共计指定字节的数据，不能多也不能少，否则行为未定义。此对象在Serial已被AsyncSerialStream托管的情况下也允许正常使用。
	properties(SetAccess=immutable,Transient)
		Serial
	end
	methods
		function obj = SerialSendSession(Length,ToPort,Serial)
			%# 语法
			% ```
			% obj=Async_stream_IO.SerialSendSession(Length,ToPort,Serial);
			% ```
			%# 输入参数
			% Length，本条消息总字节数
			% ToPort，要发送到的端口号
			% Serial(1,1)internal.Serialport，串口对象。可以用serialport获取，或者取AsyncSerialStream.Serial对象
			obj.Serial=Serial;
			Serial.write(Async_stream_IO.IAsyncStream.MagicByte,'uint8');
			Serial.write(ToPort,'uint8');
			Serial.write(Length,'uint8');
		end
		function obj = le(obj,Data)
			obj.Serial.write(typecast(Data(:),'uint8'),'uint8');
		end
	end
end