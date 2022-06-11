#include "EngineModule.h"
#include <Application/Application.h>
#include <Win64Application/Win64Application.h>
#include <Renderer/RenderCore/Renderer.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/MeshRenderer.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/RenderCore/RenderView.h>
#include <Renderer/RenderCore/GpuShader.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/View.h>
#include <RendererScene/Material.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Shader.h>
#include <RendererScene/Texture.h>

namespace ltn {
void EngineModuleManager::run() {
	ltn::win64app::Win64Application app;
	app.initialize();
	
	ltn::ApplicationSysytem* appSystem = ltn::ApplicationSysytem::Get();
	appSystem->setApplication(&app);

	Renderer* renderer = Renderer::Get();
	renderer->initialize();

	ShaderScene* shaderScene = ShaderScene::Get();
	shaderScene->initialize();

	MeshScene* meshScene = MeshScene::Get();
	meshScene->initialize();

	MeshInstanceScene* meshInstanceScene = MeshInstanceScene::Get();
	meshInstanceScene->initialize();

	ViewScene* viewScene = ViewScene::Get();
	viewScene->initialize();

	MaterialScene* materialScene = MaterialScene::Get();
	materialScene->initialize();

	TextureScene* textureScene = TextureScene::Get();
	textureScene->initialize();

	GpuTextureManager* gpuTextureManager = GpuTextureManager::Get();
	gpuTextureManager->initialize();

	RenderViewScene* renderViewScene = RenderViewScene::Get();
	renderViewScene->initialize();

	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();
	gpuShaderScene->initialize();

	GpuMeshInstanceManager* gpuMeshInstanceManager = GpuMeshInstanceManager::Get();
	gpuMeshInstanceManager->initialize();

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
	gpuMeshInstanceManager->terminate();
	meshRenderer->terminate();
	gpuShaderScene->terminate();
	renderViewScene->terminate();
	gpuTextureManager->terminate();
	textureScene->terminate();
	materialScene->terminate();
	viewScene->terminate();
	meshInstanceScene->terminate();
	shaderScene->terminate();
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
