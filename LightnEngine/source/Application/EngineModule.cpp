#include "EngineModule.h"
#include <Application/Application.h>
#include <Win64Application/Win64Application.h>
#include <Renderer/RenderCore/Renderer.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/MeshRenderer.h>
#include <Renderer/RenderCore/RenderView.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/View.h>
#include <RendererScene/Material.h>

namespace ltn {
void EngineModuleManager::run() {
	ltn::win64app::Win64Application app;
	app.initialize();
	
	ltn::ApplicationSysytem* appSystem = ltn::ApplicationSysytem::Get();
	appSystem->setApplication(&app);

	Renderer* renderer = Renderer::Get();
	renderer->initialize();

	MeshScene* meshScene = MeshScene::Get();
	meshScene->initialize();

	ViewScene* viewScene = ViewScene::Get();
	viewScene->initialize();

	MaterialScene* materialScene = MaterialScene::Get();
	materialScene->initialize();

	RenderViewScene* renderViewScene = RenderViewScene::Get();
	renderViewScene->initialize();

	GpuMeshResourceManager* gpuMeshResourceManager = GpuMeshResourceManager::Get();
	gpuMeshResourceManager->initialize();

	GeometryResourceManager* geometryResourceManager = GeometryResourceManager::Get();
	geometryResourceManager->initialize();

	GpuMaterialManager* gpuMaterialManager = GpuMaterialManager::Get();
	gpuMaterialManager->initialize();

	MeshRenderer* meshRenderer = MeshRenderer::Get();
	meshRenderer->initialize();

	app.run();

	app.terminate();
	gpuMaterialManager->terminate();
	meshRenderer->terminate();
	renderViewScene->terminate();
	materialScene->terminate();
	viewScene->terminate();
	meshScene->terminate();
	geometryResourceManager->terminate();
	gpuMeshResourceManager->terminate();
	renderer->terminate();
}

namespace {
EngineModuleManager g_engineModule;
}
EngineModuleManager* EngineModuleManager::Get() {
	return &g_engineModule;
}
}
