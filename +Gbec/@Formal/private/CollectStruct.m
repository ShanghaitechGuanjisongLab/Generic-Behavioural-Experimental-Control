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
			Value=string(UID(AsyncStream.Read));
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
			Value=seconds(AsyncStream.Read('uint16'));
		case UID.Type_Milliseconds
			Value=milliseconds(AsyncStream.Read('uint16'));
		case UID.Type_Microseconds
			Value=microseconds(AsyncStream.Read('uint16'));
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
function Array=CollectArray(Serial)
import Gbec.UID
NumElements=Serial.read(1,'uint8');
switch UID(Serial.read(1,'uint8'))
	case UID.Type_UID
		Array=UID(Serial.read(NumElements,'uint8'));
	case UID.Type_Bool
		Array=logical(Serial.read(NumElements,'uint8'));
	case UID.Type_UInt8
		Array=Serial.read(NumElements,'uint8');
	case UID.Type_UInt16
		Array=Serial.read(NumElements,'uint16');
	otherwise
		Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
end
end
function Cell=CollectCell(Serial)
import Gbec.UID
NumCells=Serial.read(1,'uint8');
Cell=cell(NumCells,1);
for C=1:NumCells
	switch UID(Serial.read(1,'uint8'))
		case UID.Type_UID
			Value=string(UID(Serial.read(1,'uint8')));
		case UID.Type_Bool
			Value=logical(Serial.read(1,'uint8'));
		case UID.Type_UInt8
			Value=Serial.read(1,'uint8');
		case UID.Type_UInt16
			Value=Serial.read(1,'uint16');
		case UID.Type_Array
			Value=CollectArray(Serial);
		case UID.Type_Cell
			Value=CollectCell(Serial);
		case UID.Type_Struct
			Value=CollectStruct(Serial);
		otherwise
			Gbec.Exceptions.Unexpected_response_from_Arduino.Throw;
	end
	Cell{C}=Value;
end
end