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
	GpuMaterialManager* materialManager = GpuMaterialManager::Get();
	u32 viewCount = ViewScene::VIEW_COUNT_MAX;
	const u8* viewEnabledFlags = viewScene->getViewEnabledFlags();
	for (u32 i = 0; i < viewCount; ++i) {
		if (viewEnabledFlags[i] == 0) {
			continue;
		}

		// GPU カリング
		{
			MeshRenderer::CullingDesc desc;
			desc._commandList = commandList;
			meshRenderer->culling(desc);
		}

		// Indirect Argument ビルド
		{
			MeshRenderer::BuildIndirectArgumentDesc desc;
			desc._commandList = commandList;
			meshRenderer->buildIndirectArgument(desc);
		}

		// 描画
		{
			MeshRenderer::RenderDesc desc;
			desc._commandList = commandList;
			desc._viewConstantDescriptor = renderViewScene->getViewConstantGpuDescriptor(i);
			desc._rootSignatures = materialManager->getRootSignatures();
			desc._pipelineStates = materialManager->getPipelineStates();
			meshRenderer->render(desc);
		}
	}
}

RenderDirector* RenderDirector::Get() {
	return &g_renderDirector;
}
}
