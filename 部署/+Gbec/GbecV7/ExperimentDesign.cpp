#include "Step.hpp"
#include <unordered_map>
//在这里注册允许远程创建的对象，及创建对象所需提供的UID，格式{UID, New<ObjectType>}

//自动结束类测试
std::unordered_map<UID,AutomaticallyEndedTest const*(*)()>const AutomaticallyEndedTests
{
};