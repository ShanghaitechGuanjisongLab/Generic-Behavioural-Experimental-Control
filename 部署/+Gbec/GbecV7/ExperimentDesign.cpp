#include "Objects.h"
const std::unordered_map<UID, Object *(*)()> ObjectCreators{
	RegisterObject(Placeholder),
};