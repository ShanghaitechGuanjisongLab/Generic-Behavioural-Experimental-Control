classdef MessageSize<uint16
	%报文大小类型，表示单个报文的字节数。最大65535字节。
	properties(Constant)
		SuperClass=string(superclasses('Async_stream_IO.MessageSize'))
	end
end