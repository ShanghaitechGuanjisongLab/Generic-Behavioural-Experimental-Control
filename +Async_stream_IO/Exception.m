classdef Exception<uint8&MATLAB.Lang.IEnumerableException
	enumeration
		Success(0)
		Argument_message_incomplete(1)
		Corrupted_object_received(2)
		Function_runtime_error(3)
		Message_received_on_allocated_port(4)
		Unlistened_port_received_message(5)
		Remote_function_have_no_return(6)
		Serial_not_respond_in_time(7)
	end
end