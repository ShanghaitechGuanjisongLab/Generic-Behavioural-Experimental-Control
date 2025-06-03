classdef Exception<uint8&MATLAB.Lang.IEnumerableException
	enumeration
		Success(0)
		Function_runtime_error(1)
		Port_idle(2)
		Port_occupied(3)
		Argument_message_incomplete(4)
		Corrupted_object_received(5)
		Message_received_on_allocated_port(6)
	end
end