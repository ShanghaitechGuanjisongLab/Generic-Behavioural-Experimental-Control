%[text] 获取上次运行的会话信息
%[text] ## 语法
%[text] ```matlabCodeExample
%[text] Information=obj.GetInformation;
%[text] %获取上次运行的会话信息
%[text] ```
%[text] ## 返回值
%[text] Information(1,1)struct，信息结构体
function Information = GetInformation(obj)
obj.Server.FeedDogIfActive;
AsyncStream=obj.Server.AsyncStream;
LocalPort=Async_stream_IO.RaiiPort(AsyncStream);
TCO=Async_stream_IO.TemporaryCallbackOff(AsyncStream);
AsyncStream.BeginSend(Gbec.UID.PortA_GetInformation,obj.Server.PointerSize+1);
AsyncStream<=LocalPort.Port<=obj.Pointer;
AsyncStream.Listen(LocalPort.Port);
Information=CollectStruct(obj.Server);
if obj.HostActions.numEntries%空字典直接取键可能会出错
	for K=obj.HostActions.keys.'
		Information.(string(K))=obj.HostActions(K).GetInformation;
	end
end
end
function Struct=CollectStruct(Server)
import Gbec.UID
AsyncStream=Server.AsyncStream;
NumFields=AsyncStream.Read;
Struct=struct;
for F=1:NumFields
	Name=AsyncStream.Read;
	Name=char(UID(Name));
	switch UID(AsyncStream.Read)
		case UID.Type_UID
			Value=categorical(string(UID(AsyncStream.Read)));
		case UID.Type_Bool
			Value=logical(AsyncStream.Read);
		case UID.Type_UInt8
			Value=AsyncStream.Read;
		case UID.Type_UInt16
			Value=AsyncStream.Read('uint16');
		case UID.Type_Array
			Value=CollectArray(Server);
		case UID.Type_Struct
			Value=CollectStruct(Server);
		case UID.Type_Seconds
			Value=seconds(AsyncStream.Read(Gbec.Formal.DurationRep));
		case UID.Type_Milliseconds
			Value=milliseconds(AsyncStream.Read(Gbec.Formal.DurationRep));
		case UID.Type_Microseconds
			Value=microseconds(AsyncStream.Read(Gbec.Formal.DurationRep));
		case UID.Type_Infinite
			Value=Inf;
		case UID.Type_Table
			Value=CollectTable(Server);
		case UID.Type_Pointer
			Value=AsyncStream.Read(Server.PointerType);
		case UID.Type_Map
			Value=CollectMap(Server);
		otherwise
			Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
	end
	Struct.(Name(strlength('Field_')+1:end))=Value;
end
end
function Array=CollectArray(Server)
import Gbec.UID
AsyncStream=Server.AsyncStream;
NumElements=AsyncStream.Read;
switch UID(AsyncStream.Read)
	case UID.Type_UID
		Array=categorical(string(UID(AsyncStream.Read(NumElements,'uint8'))));
	case UID.Type_Bool
		Array=logical(AsyncStream.Read(NumElements,'uint8'));
	case UID.Type_UInt8
		Array=AsyncStream.Read(NumElements,'uint8');
	case UID.Type_UInt16
		Array=AsyncStream.Read(NumElements,'uint16');
	case UID.Type_Seconds
		Array=seconds(AsyncStream.Read(NumElements,Gbec.Formal.DurationRep));
	case UID.Type_Milliseconds
		Array=milliseconds(AsyncStream.Read(NumElements,Gbec.Formal.DurationRep));
	case UID.Type_Microseconds
		Array=microseconds(AsyncStream.Read(NumElements,Gbec.Formal.DurationRep));
	case UID.Type_Pointer
		Array=AsyncStream.Read(NumElements,Server.PointerType);
	otherwise
		Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
end
end
function Table=CollectTable(Server)
import Gbec.UID
AsyncStream=Server.AsyncStream;
NumRows=AsyncStream.Read;
NumCols=AsyncStream.Read;
Table=table('Size',[NumRows,0]);
for C=1:NumCols
	Name=char(UID(AsyncStream.Read));
	switch UID(AsyncStream.Read)
		case UID.Type_UID
			Value=categorical(string(UID(AsyncStream.Read(NumRows))));
		case UID.Type_Bool
			Value=logical(AsyncStream.Read(NumRows));
		case UID.Type_UInt8
			Value=AsyncStream.Read(NumRows);
		case UID.Type_UInt16
			Value=AsyncStream.Read(NumRows,'uint16');
		case UID.Type_Seconds
			Value=seconds(AsyncStream.Read(NumRows,Gbec.Formal.DurationRep));
		case UID.Type_Milliseconds
			Value=milliseconds(AsyncStream.Read(NumRows,Gbec.Formal.DurationRep));
		case UID.Type_Microseconds
			Value=microseconds(AsyncStream.Read(NumRows,Gbec.Formal.DurationRep));
		case UID.Type_Pointer
			Value=AsyncStream.Read(NumRows,Server.PointerType);
		otherwise
			Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
	end
	Table.(Name(strlength('Column_')+1:end))=Value;
end
end
function Map=CollectMap(Server)
import Gbec.UID
AsyncStream=Server.AsyncStream;
NumPairs=AsyncStream.Read;
switch UID(AsyncStream.Read)
	case UID.Type_UID
		KeyReader=@(AsyncStream)categorical(string(UID(AsyncStream.Read)));
	case UID.Type_Bool
		KeyReader=@(AsyncStream)logical(AsyncStream.Read);
	case UID.Type_UInt8
		KeyReader=@(AsyncStream)AsyncStream.Read;
	case UID.Type_UInt16
		KeyReader=@(AsyncStream)AsyncStream.Read('uint16');
	case UID.Type_Seconds
		KeyReader=@(AsyncStream)seconds(AsyncStream.Read(Gbec.Formal.DurationRep));
	case UID.Type_Milliseconds
		KeyReader=@(AsyncStream)milliseconds(AsyncStream.Read(Gbec.Formal.DurationRep));
	case UID.Type_Microseconds
		KeyReader=@(AsyncStream)microseconds(AsyncStream.Read(Gbec.Formal.DurationRep));
	case UID.Type_Pointer
		KeyReader=@(AsyncStream)AsyncStream.Read(Server.PointerType);
	otherwise
		Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
end
switch UID(AsyncStream.Read)
	case UID.Type_UID
		ValueReader=@(Server)categorical(string(UID(Server.AsyncStream.Read)));
	case UID.Type_Bool
		ValueReader=@(Server)logical(Server.AsyncStream.Read);
	case UID.Type_UInt8
		ValueReader=@(Server)Server.AsyncStream.Read;
	case UID.Type_UInt16
		ValueReader=@(Server)Server.AsyncStream.Read('uint16');
	case UID.Type_Seconds
		ValueReader=@(Server)seconds(Server.AsyncStream.Read(Gbec.Formal.DurationRep));
	case UID.Type_Milliseconds
		ValueReader=@(Server)milliseconds(Server.AsyncStream.Read(Gbec.Formal.DurationRep));
	case UID.Type_Microseconds
		ValueReader=@(Server)microseconds(Server.AsyncStream.Read(Gbec.Formal.DurationRep));
	case UID.Type_Pointer
		ValueReader=@(Server)Server.AsyncStream.Read(Server.PointerType);
	case UID.Type_Struct
		ValueReader=@(Server)CollectStruct(Server);		
	otherwise
		Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
end
Map=dictionary;
for P=1:NumPairs
	Key=KeyReader(AsyncStream);
	Map(Key)=ValueReader(Server);
end
end

%[appendix]{"version":"1.0"}
%---
