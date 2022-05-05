#include "EngineModule.h"
#include <Application/Application.h>
#include <Win64Application/Win64Application.h>
#include <Renderer/RenderCore/Renderer.h>

namespace ltn {
void EngineModuleManager::run() {
	ltn::win64app::Win64Application app;
	app.initialize();
	
	ltn::ApplicationSysytem* appSystem = ltn::ApplicationSysytem::Get();
	appSystem->setApplication(&app);

	Renderer* renderer = Renderer::Get();
	renderer->initialize();

	app.run();

	app.terminate();
	renderer->terminate();
}

namespace {
EngineModuleManager g_engineModule;
}
EngineModuleManager* EngineModuleManager::Get() {
	return &g_engineModule;
}
}
