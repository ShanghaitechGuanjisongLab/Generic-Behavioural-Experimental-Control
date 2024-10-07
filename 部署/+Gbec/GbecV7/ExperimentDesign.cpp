#include "Objects.h"
//在这里注册允许远程创建的对象，及创建对象所需提供的UID，格式{UID, New<ObjectType>}
const std::unordered_map<UID, Object *(*)()> ObjectCreators{
};