#include <Application/EngineModule.h>
#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	ltn::EngineModuleManager* engine = ltn::EngineModuleManager::Get();
	engine->run();

	return 0;
}