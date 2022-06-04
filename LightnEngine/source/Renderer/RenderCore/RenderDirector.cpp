#include "RenderDirector.h"
#include <Renderer/RenderCore/RenderView.h>
#include <Renderer/MeshRenderer/MeshRenderer.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <RendererScene/Material.h>
#include <RendererScene/View.h>

namespace ltn {
namespace {
RenderDirector g_renderDirector;
}

void RenderDirector::initialize() {
}

void RenderDirector::terminate() {
}

void RenderDirector::render(rhi::CommandList* commandList) {
	ViewScene* viewScene = ViewScene::Get();
	RenderViewScene* renderViewScene = RenderViewScene::Get();
	MeshRenderer* meshRenderer = MeshRenderer::Get();
	MaterialScene* materialScene = MaterialScene::Get();
	GpuMaterialManager* materialManager = GpuMaterialManager::Get();
	u32 viewCount = ViewScene::VIEW_COUNT_MAX;
	const View* views = viewScene->getView(0);
	const u8* viewEnabledFlags = viewScene->getViewEnabledFlags();
	for (u32 i = 0; i < viewCount; ++i) {
		if (viewEnabledFlags[i] == 0) {
			continue;
		}

		// GPU ƒJƒŠƒ“ƒO
		{
			MeshRenderer::CullingDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			meshRenderer->culling(desc);
		}

		// •`‰æ
		{
			const View& view = views[i];
			renderViewScene->setUpView(commandList, view, i);

			MeshRenderer::RenderDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._rootSignatures = materialManager->getDefaultRootSignatures();
			desc._pipelineStates = materialManager->getDefaultPipelineStates();
			desc._pipelineStateCount = MaterialScene::MATERIAL_COUNT_MAX;
			desc._enabledFlags = materialScene->getEnabledFlags();
			meshRenderer->render(desc);
		}
	}
}

RenderDirector* RenderDirector::Get() {
	return &g_renderDirector;
}
}
