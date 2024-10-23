function Struct=CollectStruct(Serial)
import Gbec.UID
NumFields=Serial.read(1,'uint8');
Struct=struct;
for F=1:NumFields
	Name=Serial.read(1,'uint8');
	Name=char(UID(Name));
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
			Gbec.Exception.Unexpected_response_from_Arduino.Throw;
	end
	Struct.(Name(strlength('Info_')+1:end))=Value;
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
		Gbec.Exception.Unexpected_response_from_Arduino.Throw;
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
			Gbec.Exception.Unexpected_response_from_Arduino.Throw;
	end
	Cell{C}=Value;
end
end