#include "RenderDirector.h"
#include <Renderer/RenderCore/RenderView.h>
#include <Renderer/MeshRenderer/GpuCulling.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/VisibilityBufferRenderer.h>
#include <Renderer/MeshRenderer/ComputeLod.h>
#include <RendererScene/Material.h>
#include <RendererScene/View.h>

#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/MeshRenderer/IndirectArgumentResource.h>
#include <ThiredParty/ImGui/imgui.h>

namespace ltn {
namespace {
RenderDirector g_renderDirector;
}

void RenderDirector::initialize() {
}

void RenderDirector::terminate() {
}

void RenderDirector::update() {
	ImGui::Begin("DebugVisualize");

	const char* names[] = {
		"None",
		"MeshInstanceLodLevels",
		"MeshInstanceScreenPersentages",
		"MeshInstanceIndex",
		"WorldPosition",
		"Texcoords",
		"Primitive",
	};
	ImGui::Combo("Type", &_debugVisualizeType, names, LTN_COUNTOF(names));
	ImGui::End();
}

void RenderDirector::render(rhi::CommandList* commandList) {
	ViewScene* viewScene = ViewScene::Get();
	RenderViewScene* renderViewScene = RenderViewScene::Get();
	GpuCulling* gpuCulling = GpuCulling::Get();
	ComputeLod* computeLod = ComputeLod::Get();
	VisiblityBufferRenderer* visibilityBufferRenderer = VisiblityBufferRenderer::Get();
	PipelineSetScene* pipelineSetScene = PipelineSetScene::Get();
	GpuMaterialManager* materialManager = GpuMaterialManager::Get();
	u32 viewCount = ViewScene::VIEW_COUNT_MAX;
	const View* views = viewScene->getView(0);
	const u8* viewEnabledFlags = viewScene->getViewEnabledFlags();

	rhi::DescriptorHeap* descriptorHeaps[] = { DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->getDescriptorHeap() };
	commandList->setDescriptorHeaps(LTN_COUNTOF(descriptorHeaps), descriptorHeaps);

	IndirectArgumentResource indirectArgumentResource;
	RenderViewFrameResource renderViewFrameResource;
	VisibilityBufferFrameResource visibilityBufferFrameResource;

	for (u32 i = 0; i < viewCount; ++i) {
		if (viewEnabledFlags[i] == 0) {
			continue;
		}

		// フレームリソース初期化
		{
			indirectArgumentResource.setUpFrameResource(commandList);
			renderViewFrameResource.setUpFrameResource(&views[i], commandList);
			visibilityBufferFrameResource.setUpFrameResource(commandList);
		}

		// カリング結果リセット
		{
			renderViewScene->resetCullingResult(commandList, &renderViewFrameResource, i);
		}

		// LOD 情報計算
		{
			ComputeLod::ComputeLodDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._cullingInfoCbv = gpuCulling->getCullingInfoCbv();
			computeLod->computeLod(desc);
		}

		// GPU カリング
		{
			GpuCulling::CullingDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._cullingResultUav = renderViewFrameResource._cullingResultUav._gpuHandle;
			desc._indirectArgumentResource = &indirectArgumentResource;
			gpuCulling->gpuCulling(desc);
		}

		// シェーダー ID クリア
		{
			VisiblityBufferRenderer::ClearShaderIdDesc desc;
			desc._commandList = commandList;
			desc._frameResource = &visibilityBufferFrameResource;
			visibilityBufferRenderer->clearShaderId(desc);
		}

		// ジオメトリパス
		{
			const View& view = views[i];
			renderViewScene->setViewports(commandList, view, i);

			VisiblityBufferRenderer::GeometryPassDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._viewDsv = renderViewFrameResource._viewDsv._cpuHandle;
			desc._rootSignatures = materialManager->getGeometryPassRootSignatures();
			desc._pipelineStates = materialManager->getGeometryPassPipelineStates();
			desc._pipelineStateCount = PipelineSetScene::PIPELINE_SET_CAPACITY;
			desc._enabledFlags = pipelineSetScene->getEnabledFlags();
			desc._indirectArgumentResource = &indirectArgumentResource;
			desc._frameResource = &visibilityBufferFrameResource;
			visibilityBufferRenderer->geometryPass(desc);
		}

		// シェーダー ID 構築
		{
			VisiblityBufferRenderer::BuildShaderIdDesc desc;
			desc._commandList = commandList;
			desc._frameResource = &visibilityBufferFrameResource;
			visibilityBufferRenderer->buildShaderId(desc);
		}

		// シェーディング
		{
			VisiblityBufferRenderer::ShadingPassDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._viewRtv = renderViewFrameResource._viewRtv._cpuHandle;
			desc._rootSignatures = materialManager->getShadingPassRootSignatures();
			desc._pipelineStates = materialManager->getShadingPassPipelineStates();
			if (_debugVisualizeType > 0) {
				desc._pipelineStates = materialManager->getDebugShadingPassPipelineStates();
			}
			desc._debugVisualizeType = _debugVisualizeType;
			desc._pipelineStateCount = PipelineSetScene::PIPELINE_SET_CAPACITY;
			desc._enabledFlags = pipelineSetScene->getEnabledFlags();
			desc._frameResource = &visibilityBufferFrameResource;
			visibilityBufferRenderer->shadingPass(desc);
		}

		// 0 番ビューをメインビューとして設定
		if (i == 0) {
			renderViewScene->setMainViewGpuTexture(renderViewFrameResource._viewColorTexture);
		}
	}
}

RenderDirector* RenderDirector::Get() {
	return &g_renderDirector;
}
}
