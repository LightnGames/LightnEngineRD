#include "RenderView.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <RendererScene/View.h>
#include <ThiredParty/ImGui/imgui.h>

namespace ltn {
namespace {
RenderViewScene g_renderViewScene;
}
void RenderViewScene::initialize() {
	// デスクリプタのみ初期化時に確保しておく
	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	_viewCbv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewSrv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewRtv = descriptorAllocatorGroup->getRtvAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewDsv = descriptorAllocatorGroup->getDsvAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewDepthSrv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_cullingResultUav = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_cullingResultCpuUav = descriptorAllocatorGroup->getSrvCbvUavCpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
}

void RenderViewScene::terminate() {
	update();
	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewCbv);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewSrv);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewDepthSrv);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_cullingResultUav);
	descriptorAllocatorGroup->getSrvCbvUavCpuAllocator()->free(_cullingResultCpuUav);
	descriptorAllocatorGroup->getRtvAllocator()->free(_viewRtv);
	descriptorAllocatorGroup->getDsvAllocator()->free(_viewDsv);
}

void RenderViewScene::update() {
	ViewScene* viewScene = ViewScene::Get();
	const View* views = viewScene->getView(0);
	const u8* stateFlags = viewScene->getViewStateFlags();
	for (u32 i = 0; i < ViewScene::VIEW_COUNT_MAX; ++i) {
		if (stateFlags[i] == View::VIEW_STATE_NONE) {
			continue;
		}

		// ビュー生成
		if (stateFlags[i] & View::VIEW_STATE_CREATED) {
			rhi::Device* device = DeviceManager::Get()->getDevice();
			const View& view = views[i];

			// カラーテクスチャ
			{
				rhi::ClearValue optimizedClearValue = {};
				optimizedClearValue._format = rhi::BACK_BUFFER_FORMAT;

				GpuTextureDesc desc = {};
				desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
				desc._device = device;
				desc._format = rhi::BACK_BUFFER_FORMAT;
				desc._optimizedClearValue = &optimizedClearValue;
				desc._width = view.getWidth();
				desc._height = view.getHeight();
				desc._flags = rhi::RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				desc._initialState = rhi::RESOURCE_STATE_RENDER_TARGET;
				_viewColorTextures[i].initialize(desc);
				_viewColorTextures[i].setName("ViewColor[%d]", i);
			}

			// デプステクスチャ
			{
				rhi::ClearValue depthOptimizedClearValue = {};
				depthOptimizedClearValue._format = rhi::FORMAT_D32_FLOAT;
				depthOptimizedClearValue._depthStencil._depth = 1.0f;
				depthOptimizedClearValue._depthStencil._stencil = 0;

				GpuTextureDesc desc = {};
				desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
				desc._device = device;
				desc._format = rhi::FORMAT_D32_FLOAT;
				desc._optimizedClearValue = &depthOptimizedClearValue;
				desc._width = view.getWidth();
				desc._height = view.getHeight();
				desc._flags = rhi::RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				desc._initialState = rhi::RESOURCE_STATE_DEPTH_WRITE;
				_viewDepthTextures[i].initialize(desc);
				_viewDepthTextures[i].setName("ViewDepth[%d]", i);
			}

			// 定数バッファ
			{
				GpuBufferDesc desc = {};
				desc._device = device;
				desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
				desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				desc._sizeInByte = rhi::GetConstantBufferAligned(sizeof(gpu::View));
				_viewConstantBuffers[i].initialize(desc);
				_viewConstantBuffers[i].setName("ViewConstant[%d]", i);
			}

			// カリング結果
			{
				GpuBufferDesc desc = {};
				desc._device = device;
				desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
				desc._heapType = rhi::HEAP_TYPE_DEFAULT;
				desc._initialState = rhi::RESOURCE_STATE_UNORDERED_ACCESS;
				desc._flags = rhi::RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				desc._sizeInByte = sizeof(gpu::CullingResult);
				_cullingResultGpuBuffers[i].initialize(desc);
				_cullingResultGpuBuffers[i].setName("CullingResult[%d]", i);

				// MEMO: サイズが小さくてアライメント的に不利かもしれない
				desc._initialState = rhi::RESOURCE_STATE_COPY_DEST;
				desc._heapType = rhi::HEAP_TYPE_READBACK;
				desc._flags = rhi::RESOURCE_FLAG_NONE;
				desc._allocator = nullptr;
				_cullingResultReadbackBuffer[i].initialize(desc);
				_cullingResultReadbackBuffer[i].setName("CullingResultReadback[%d]", i);
			}

			// 定数バッファビュー
			{
				rhi::ConstantBufferViewDesc desc;
				desc._bufferLocation = _viewConstantBuffers[i].getGpuVirtualAddress();
				desc._sizeInBytes = _viewConstantBuffers[i].getSizeInByte();
				device->createConstantBufferView(desc, _viewCbv.get(i)._cpuHandle);
			}

			// デスクリプター
			{
				device->createDepthStencilView(_viewDepthTextures[i].getResource(), _viewDsv.get(i)._cpuHandle);
				device->createRenderTargetView(_viewColorTextures[i].getResource(), _viewRtv.get(i)._cpuHandle);
				device->createShaderResourceView(_viewColorTextures[i].getResource(), nullptr, _viewSrv.get(i)._cpuHandle);

				rhi::ShaderResourceViewDesc srvDesc = {};
				srvDesc._format = rhi::FORMAT_R32_FLOAT;
				srvDesc._viewDimension = rhi::SRV_DIMENSION_TEXTURE2D;
				srvDesc._texture2D._mipLevels = 1;
				device->createShaderResourceView(_viewDepthTextures[i].getResource(), &srvDesc, _viewDepthSrv.get(i)._cpuHandle);

				rhi::UnorderedAccessViewDesc uavDesc = {};
				uavDesc._viewDimension = rhi::UAV_DIMENSION_BUFFER;
				uavDesc._format = rhi::FORMAT_R32_TYPELESS;
				uavDesc._buffer._flags = rhi::BUFFER_UAV_FLAG_RAW;
				uavDesc._buffer._numElements = _cullingResultGpuBuffers[i].getU32ElementCount();
				device->createUnorderedAccessView(_cullingResultGpuBuffers[i].getResource(), nullptr, &uavDesc, _cullingResultUav.get(i)._cpuHandle);
				device->createUnorderedAccessView(_cullingResultGpuBuffers[i].getResource(), nullptr, &uavDesc, _cullingResultCpuUav.get(i)._cpuHandle);
			}

			// 初期化時は Gpu ビュー情報も初期化
			updateGpuView(views[i], i);
		}

		// ビュー更新
		if (stateFlags[i] & View::VIEW_STATE_UPDATED) {
			updateGpuView(views[i], i);
		}

		// ビュー破棄
		if (stateFlags[i] & View::VIEW_STATE_DESTROY) {
			_viewColorTextures[i].terminate();
			_viewDepthTextures[i].terminate();
			_viewConstantBuffers[i].terminate();
			_cullingResultGpuBuffers[i].terminate();
			_cullingResultReadbackBuffer[i].terminate();
		}

		// カリング結果デバッグ表示
		showCullingResultStatus(i);
	}
}

void RenderViewScene::setUpView(rhi::CommandList* commandList, const View& view, u32 viewIndex) {
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "SetUpView");
	f32 clearColor[4] = {};
	rhi::CpuDescriptorHandle rtv = _viewRtv.get(viewIndex)._cpuHandle;
	rhi::CpuDescriptorHandle dsv = _viewDsv.get(viewIndex)._cpuHandle;
	rhi::ViewPort viewPort = createViewPort(view);
	rhi::Rect scissorRect = createScissorRect(view);
	commandList->setRenderTargets(1, &rtv, &dsv);
	commandList->clearRenderTargetView(rtv, clearColor);
	commandList->clearDepthStencilView(dsv, rhi::CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->setViewports(1, &viewPort);
	commandList->setScissorRects(1, &scissorRect);
}

void RenderViewScene::resetCullingResult(rhi::CommandList* commandList, u32 viewIndex) {
	// カリング結果をリードバックバッファにコピー
	GpuBuffer& readbackBuffer = _cullingResultReadbackBuffer[viewIndex];
	{
		ScopedBarrierDesc barriers[] = {
	        ScopedBarrierDesc(&_cullingResultGpuBuffers[viewIndex], rhi::RESOURCE_STATE_COPY_SOURCE),
		};
		ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));
		commandList->copyResource(readbackBuffer.getResource(), _cullingResultGpuBuffers[viewIndex].getResource());
	}

	// リードバックバッファから CPU メモリにコピー
	const gpu::CullingResult* cullingResult = readbackBuffer.map<gpu::CullingResult>();
	_cullingResult[viewIndex] = *cullingResult;
	readbackBuffer.unmap();

	// クリア
	u32 clearValues[4] = {};
	rhi::GpuDescriptorHandle gpuDescriptor = _cullingResultUav.get(viewIndex)._gpuHandle;
	rhi::CpuDescriptorHandle cpuDescriptor = _cullingResultCpuUav.get(viewIndex)._cpuHandle;
	rhi::Resource* resource = _cullingResultGpuBuffers[viewIndex].getResource();
	commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, resource, clearValues, 0, nullptr);
}

RenderViewScene* RenderViewScene::Get() {
	return &g_renderViewScene;
}

void RenderViewScene::updateGpuView(const View& view, u32 viewIndex) {
	const Camera* camera = view.getCamera();
	f32 farClip = camera->_farClip;
	f32 nearClip = camera->_nearClip;
	f32 fov = camera->_fov;
	f32 fovHalfTan = Tan(fov / 2.0f);
	f32 aspectRate = view.getAspectRate();
	Vector3 cameraPosition = camera->_worldMatrix.getCol(3).getVector3();
	Matrix4 viewMatrix = camera->_worldMatrix.inverse();
	Matrix4 projectionMatrix = Matrix4::perspectiveFovLH(fov, aspectRate, nearClip, farClip);
	Matrix4 viewProjectionMatrix = viewMatrix * projectionMatrix;

	gpu::View* gpuView = VramUpdater::Get()->enqueueUpdate<gpu::View>(&_viewConstantBuffers[viewIndex]);
	gpuView->_matrixView = viewMatrix.transpose().getFloat4x4();
	gpuView->_matrixProj = projectionMatrix.transpose().getFloat4x4();
	gpuView->_matrixViewProj = viewProjectionMatrix.transpose().getFloat4x4();
	gpuView->_cameraPosition = cameraPosition.getFloat3();
	gpuView->_nearAndFarClip.x = nearClip;
	gpuView->_nearAndFarClip.y = farClip;
	gpuView->_halfFovTan.x = fovHalfTan * aspectRate;
	gpuView->_halfFovTan.y = fovHalfTan;
	gpuView->_viewPortSize[0] = view.getWidth();
	gpuView->_viewPortSize[1] = view.getHeight();
	gpuView->_upDirection = camera->_worldMatrix.getCol(1).getFloat3();
}

void RenderViewScene::showCullingResultStatus(u32 viewIndex) const {
	ImGui::Begin("CullingResult");
	constexpr char FORMAT[] = "%-20s : %-10s";
	const gpu::CullingResult& cullingResult = _cullingResult[viewIndex];
	ImGui::Text("MeshInstance");
	ImGui::Text(FORMAT, "TestFrustumCulling", ThreeDigiets(cullingResult._testFrustumCullingMeshInstanceCount).get());
	ImGui::Text(FORMAT, "PassFrustumCulling", ThreeDigiets(cullingResult._passFrustumCullingMeshInstanceCount).get());
	ImGui::Text(FORMAT, "PassOcclusionCulling", ThreeDigiets(cullingResult._passOcclusionCullingMeshInstanceCount).get());
	ImGui::Separator();
	ImGui::Text("SubMeshInstance");
	ImGui::Text(FORMAT, "TestFrustumCulling", ThreeDigiets(cullingResult._testFrustumCullingSubMeshInstanceCount).get());
	ImGui::Text(FORMAT, "PassFrustumCulling", ThreeDigiets(cullingResult._passFrustumCullingSubMeshInstanceCount).get());
	ImGui::Text(FORMAT, "PassOcclusionCulling", ThreeDigiets(cullingResult._passOcclusionCullingSubMeshInstanceCount).get());
	ImGui::Separator();
	ImGui::Text("Triangle");
	ImGui::Text(FORMAT, "TestFrustumCulling", ThreeDigiets(cullingResult._testFrustumCullingTriangleCount).get());
	ImGui::Text(FORMAT, "PassFrustumCulling", ThreeDigiets(cullingResult._passFrustumCullingTriangleCount).get());
	ImGui::Text(FORMAT, "PassOcclusionCulling", ThreeDigiets(cullingResult._passOcclusionCullingTriangleCount).get());
	ImGui::End();
}
}
