#include "RenderDirector.h"
#include <Renderer/RenderCore/RenderView.h>
#include <Renderer/MeshRenderer/GpuCulling.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/VisibilityBufferRenderer.h>
#include <Renderer/MeshRenderer/ComputeLod.h>
#include <RendererScene/Material.h>
#include <RendererScene/View.h>

namespace ltn {
namespace {
RenderDirector g_renderDirector;
}

void RenderDirector::initialize() {
	_indirectArgumentResource.initialize();
}

void RenderDirector::terminate() {
	_indirectArgumentResource.terminate();
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

	for (u32 i = 0; i < viewCount; ++i) {
		if (viewEnabledFlags[i] == 0) {
			continue;
		}

		// �J�����O���ʃ��Z�b�g
		{
			renderViewScene->resetCullingResult(commandList, i);
		}

		// LOD ���v�Z
		{
			ComputeLod::ComputeLodDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._cullingInfoCbv = gpuCulling->getCullingInfoCbv();
			computeLod->computeLod(desc);
		}

		// GPU �J�����O
		{
			GpuCulling::CullingDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._cullingResultUav = renderViewScene->getCullingResultUav(i);
			desc._indirectArgumentResource = &_indirectArgumentResource;
			gpuCulling->gpuCulling(desc);
		}

		// �V�F�[�_�[ ID �N���A
		{
			VisiblityBufferRenderer::ClearShaderIdDesc desc;
			desc._commandList = commandList;
			visibilityBufferRenderer->clearShaderId(desc);
		}

		// �W�I���g���p�X
		{
			const View& view = views[i];
			renderViewScene->setViewports(commandList, view, i);

			VisiblityBufferRenderer::GeometryPassDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._viewDsv = renderViewScene->getViewCpuDsv(i);
			desc._rootSignatures = materialManager->getGeometryPassRootSignatures();
			desc._pipelineStates = materialManager->getGeometryPassPipelineStates();
			desc._pipelineStateCount = PipelineSetScene::PIPELINE_SET_CAPACITY;
			desc._enabledFlags = pipelineSetScene->getEnabledFlags();
			desc._indirectArgumentResource = &_indirectArgumentResource;
			visibilityBufferRenderer->geometryPass(desc);
		}

		// �V�F�[�_�[ ID �\�z
		{
			VisiblityBufferRenderer::BuildShaderIdDesc desc;
			desc._commandList = commandList;
			visibilityBufferRenderer->buildShaderId(desc);
		}

		// �V�F�[�f�B���O
		{
			VisiblityBufferRenderer::ShadingPassDesc desc;
			desc._commandList = commandList;
			desc._viewCbv = renderViewScene->getViewCbv(i);
			desc._viewRtv = renderViewScene->getViewCpuRtv(i);
			desc._rootSignatures = materialManager->getShadingPassRootSignatures();
			desc._pipelineStates = materialManager->getShadingPassPipelineStates();
			desc._pipelineStateCount = PipelineSetScene::PIPELINE_SET_CAPACITY;
			desc._enabledFlags = pipelineSetScene->getEnabledFlags();
			visibilityBufferRenderer->shadingPass(desc);
		}
	}
}

RenderDirector* RenderDirector::Get() {
	return &g_renderDirector;
}
}
