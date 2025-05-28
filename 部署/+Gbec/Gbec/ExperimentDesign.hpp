#include "BuildingBlocks.hpp"

//使用using为任何步骤赋予别名，以便重用

using PublicSteps = std::tuple<
	// 在这里注册允许远程创建的公开步骤，及创建对象所需提供的UID。格式：Pair<UID,步骤>，英文逗号分隔。例如：
	Pair<UID::Public_BlueLed, Sequential<DigitalWrite<7, HIGH>, Delay<Milliseconds<200>>, DigitalWrite<7, LOW>>>>;