#include <Cpp_Standard_Library.h>
#include <vector>
#include <unordered_map>
struct Module
{
	void Start(size_t ArgumentBytes, void *Arguments);
	void Pause();
	void Continue();
	void Kill();
	void Info(std::vector<char>&);
};