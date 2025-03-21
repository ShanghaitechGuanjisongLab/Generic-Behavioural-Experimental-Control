classdef Exception<MATLAB.Lang.IEnumerableException
	enumeration
		Serial_handshake_failed
		Test_not_found_on_Arduino
		Unexpected_response_from_Arduino
		Last_test_not_running_or_unstoppable
		Session_not_found_on_Arduino
		No_sessions_are_running
		Cannot_test_while_session_running
		Cannot_test_while_session_paused
		Cannot_pause_a_paused_session
		Cannot_pause_an_aborted_session
		Cannot_pause_a_finished_session
		Cannot_continue_a_running_session
		Cannot_continue_an_aborted_session
		Cannot_continue_a_finished_session
		Cannot_abort_an_aborted_session
		Cannot_abort_a_finished_session
		Must_run_session_before_getting_information
		There_is_already_a_session_running
		There_is_already_a_session_being_paused
		Disconnection_reconnection_failed
		Cannot_OneEnterOneCheck_on_manually_stopped_test
		Arduino_received_unsupported_API_code
		Cannot_get_information_while_session_running
		Cannot_record_without_VideoInput
		Please_install_AVR_board_platform_in_Arduino_IDE_first
		Unsupported_NET_environment_please_restart_MATLAB
		Arduino_requires_undefined_HostAction
		Grating_properties_invalid
		No_right_to_write_SavePath
		User_canceled_operation
		Serialport_disconnected
		Failure_to_merge_existing_dataset
		Serialport_not_found_in_list
	end
end