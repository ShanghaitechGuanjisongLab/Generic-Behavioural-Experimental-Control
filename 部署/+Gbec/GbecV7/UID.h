enum class UID : uint8_t
{
	Signal_SerialReady,
	Command_StartProcess,
	Command_PauseProcess,
	Command_ContinueProcess,
	Command_KillProcess,
	Command_RunningProcesses,
	Command_ModuleInfo,
};
template <typename T>
inline void SerialWrite(T Value)
{
	Serial.write(reinterpret_cast<char *> & T, sizeof(T));
}
template <typename T>
inline void SerialRead(T &Value)
{
	Serial.readBytes(reinterpret_cast<char *> & Value, sizeof(T));
}
template <typename T>
inline T SerialRead()
{
	T Value;
	Serial.readBytes(reinterpret_cast<char *> & Value, sizeof(T));
	return Value;
}