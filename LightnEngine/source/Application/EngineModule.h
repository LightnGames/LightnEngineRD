#pragma once
#include <Core/ModuleSettings.h>

namespace ltn {
class LTN_API EngineModuleManager{
public:
	void run();

	static EngineModuleManager* Get();
};
}