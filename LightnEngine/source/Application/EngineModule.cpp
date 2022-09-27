#include "EngineModule.h"
#include <Core/CpuTimerManager.h>
#include <Core/Level.h>
#include <Core/InputSystem.h>
#include <Application/Application.h>
#include <Win64Application/Win64Application.h>
#include <Renderer/RenderCore/Renderer.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/GpuCulling.h>
#include <Renderer/MeshRenderer/ComputeLod.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/MeshRenderer/VisibilityBufferRenderer.h>
#include <Renderer/RenderCore/RenderView.h>
#include <Renderer/RenderCore/GpuShader.h>
#include <Renderer/RenderCore/LodStreamingManager.h>
#include <Renderer/Lighting/SkySphereRenderer.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/View.h>
#include <RendererScene/Material.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Shader.h>
#include <RendererScene/Texture.h>
#include <RendererScene/SkySphere.h>
#include <RendererScene/CommonResource.h>

namespace ltn {
void update() {
	InputSystem::Get()->update();
	CpuTimerManager::Get()->update();
	LodStreamingManager::Get()->update();
	GpuTextureManager::Get()->update();
	GpuShaderScene::Get()->update();
	RenderViewScene::Get()->update();
	GpuMeshInstanceManager::Get()->update();
	GpuMeshResourceManager::Get()->update();
	GeometryResourceManager::Get()->update();
	GpuMaterialManager::Get()->update();
	GpuCulling::Get()->update();

	SkySphereScene::Get()->lateUpdate();
	TextureScene::Get()->lateUpdate();
	MaterialScene::Get()->lateUpdate();
	MeshInstanceScene::Get()->lateUpdate();
	MeshGeometryScene::Get()->lateUpdate();
	ShaderScene::Get()->lateUpdate();
	ViewScene::Get()->lateUpdate();
	Renderer::Get()->update();
}

void render() {
	Renderer::Get()->render();
}

void EngineModuleManager::run() {
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	ltn::win64app::Win64Application app;
	EditorCamera editorCamera;
	Level level;

	ltn::ApplicationSysytem* appSystem = ltn::ApplicationSysytem::Get();
	CpuTimerManager* cpuTimerManager = CpuTimerManager::Get();
	Renderer* renderer = Renderer::Get();
	ShaderScene* shaderScene = ShaderScene::Get();
	MeshGeometryScene* meshGeometryScene = MeshGeometryScene::Get();
	MeshInstanceScene* meshInstanceScene = MeshInstanceScene::Get();
	ViewScene* viewScene = ViewScene::Get();
	MaterialScene* materialScene = MaterialScene::Get();
	TextureScene* textureScene = TextureScene::Get();
	MeshScene* meshScene = MeshScene::Get();
	GpuTextureManager* gpuTextureManager = GpuTextureManager::Get();
	RenderViewScene* renderViewScene = RenderViewScene::Get();
	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();
	GpuMeshInstanceManager* gpuMeshInstanceManager = GpuMeshInstanceManager::Get();
	GpuMeshResourceManager* gpuMeshResourceManager = GpuMeshResourceManager::Get();
	GeometryResourceManager* geometryResourceManager = GeometryResourceManager::Get();
	GpuMaterialManager* gpuMaterialManager = GpuMaterialManager::Get();
	LodStreamingManager* meshLodStreamingManager = LodStreamingManager::Get();
	GpuCulling* gpuCulling = GpuCulling::Get();
	ComputeLod* computeLod = ComputeLod::Get();
	VisiblityBufferRenderer* visibilityBufferRenderer = VisiblityBufferRenderer::Get();
	SkySphereRenderer* skySphereRenderer = SkySphereRenderer::Get();
	SkySphereScene* skySphereScene = SkySphereScene::Get();
	CommonResource* commonResource = CommonResource::Get();

	{
		CpuScopedPerf scopedPerf("-- EngineInitializetion --");

		app.initialize();
		appSystem->setApplication(&app);
		cpuTimerManager->initialize();
		renderer->initialize();
		shaderScene->initialize();
		meshGeometryScene->initialize();
		meshInstanceScene->initialize();
		viewScene->initialize();
		materialScene->initialize();
		textureScene->initialize();
		meshScene->initialize();
		gpuTextureManager->initialize();
		renderViewScene->initialize();
		gpuShaderScene->initialize();
		gpuMeshInstanceManager->initialize();
		gpuMeshResourceManager->initialize();
		geometryResourceManager->initialize();
		gpuMaterialManager->initialize();
		meshLodStreamingManager->initialize();
		gpuCulling->initialize();
		computeLod->initialize();
		visibilityBufferRenderer->initialize();
		skySphereScene->initialize();
		skySphereRenderer->initialize();

		commonResource->initialize();
		editorCamera.initialize();
		level.initialize("GohicHorror\\level.level");
	}

	while (app.update()) {
		editorCamera.update();
		update();
		render();
	}

	{
		CpuScopedPerf scopedPerf("-- EngineTermination --");

		level.terminate();
		editorCamera.terminate();
		commonResource->terminate();

		// 終了する前に GPU 処理待ち
		{
			Renderer::Get()->waitForIdle();
			update();
		}

		app.terminate();
		gpuMaterialManager->terminate();
		gpuMeshInstanceManager->terminate();
		skySphereRenderer->terminate();
		skySphereScene->terminate();
		visibilityBufferRenderer->terminate();
		computeLod->terminate();
		gpuCulling->terminate();
		gpuShaderScene->terminate();
		renderViewScene->terminate();
		gpuTextureManager->terminate();
		textureScene->terminate();
		materialScene->terminate();
		viewScene->terminate();
		meshInstanceScene->terminate();
		shaderScene->terminate();
		meshGeometryScene->terminate();
		meshScene->terminate();
		geometryResourceManager->terminate();
		gpuMeshResourceManager->terminate();
		meshLodStreamingManager->terminate();
		renderer->terminate();
		cpuTimerManager->terminate();
	}

	// メモリリーク情報をダンプ
	// TODO: Threadを新しく作るとそこの中身がメモリリークとして出る。PipelineStateReloader
	_CrtDumpMemoryLeaks();
}

namespace {
EngineModuleManager g_engineModule;
}
EngineModuleManager* EngineModuleManager::Get() {
	return &g_engineModule;
}
}
