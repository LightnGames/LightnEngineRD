#include "EngineModule.h"
#include <Application/Application.h>
#include <Win64Application/Win64Application.h>
#include <Renderer/RenderCore/Renderer.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <RendererScene/Mesh.h>

namespace ltn {
void EngineModuleManager::run() {
	ltn::win64app::Win64Application app;
	app.initialize();
	
	ltn::ApplicationSysytem* appSystem = ltn::ApplicationSysytem::Get();
	appSystem->setApplication(&app);

	MeshScene* meshScene = MeshScene::Get();
	meshScene->initialize();

	Renderer* renderer = Renderer::Get();
	renderer->initialize();

	GpuMeshResourceManager* gpuMeshResourceManager = GpuMeshResourceManager::Get();
	gpuMeshResourceManager->initialize();

	app.run();

	app.terminate();
	gpuMeshResourceManager->terminate();
	renderer->terminate();
	meshScene->terminate();
}

namespace {
EngineModuleManager g_engineModule;
}
EngineModuleManager* EngineModuleManager::Get() {
	return &g_engineModule;
}
}
