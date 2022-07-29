#include "RenderView.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/FrameResourceAllocator.h>
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
}

void RenderViewScene::terminate() {
	update();
	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewCbv);
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
				// MEMO: サイズが小さくてアライメント的に不利かもしれない
				GpuBufferDesc desc = {};
				desc._device = device;
				desc._sizeInByte = GetAligned(sizeof(gpu::CullingResult), rhi::DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				desc._initialState = rhi::RESOURCE_STATE_COPY_DEST;
				desc._heapType = rhi::HEAP_TYPE_READBACK;
				desc._flags = rhi::RESOURCE_FLAG_NONE;
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

			// 初期化時は Gpu ビュー情報も初期化
			updateGpuView(views[i], i);
		}

		// ビュー更新
		if (stateFlags[i] & View::VIEW_STATE_UPDATED) {
			updateGpuView(views[i], i);
		}

		// ビュー破棄
		if (stateFlags[i] & View::VIEW_STATE_DESTROY) {
			_viewConstantBuffers[i].terminate();
			_cullingResultReadbackBuffer[i].terminate();
		}

		// カリング結果デバッグ表示
		showCullingResultStatus(i);
	}
}

void RenderViewScene::setViewports(rhi::CommandList* commandList, const View& view, u32 viewIndex) {
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "SetUpView");
	rhi::ViewPort viewPort = createViewPort(view);
	rhi::Rect scissorRect = createScissorRect(view);
	commandList->setViewports(1, &viewPort);
	commandList->setScissorRects(1, &scissorRect);
}

void RenderViewScene::resetCullingResult(rhi::CommandList* commandList, RenderViewFrameResource* frameResource, u32 viewIndex) {
	// カリング結果をリードバックバッファにコピー
	GpuBuffer& readbackBuffer = _cullingResultReadbackBuffer[viewIndex];
	{
		ScopedBarrierDesc barriers[] = {
			ScopedBarrierDesc(frameResource->_cullingResultGpuBuffer, rhi::RESOURCE_STATE_COPY_SOURCE),
		};
		ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));
		commandList->copyResource(readbackBuffer.getResource(), frameResource->_cullingResultGpuBuffer->getResource());
	}

	// リードバックバッファから CPU メモリにコピー
	const gpu::CullingResult* cullingResult = readbackBuffer.map<gpu::CullingResult>();
	_cullingResult[viewIndex] = *cullingResult;
	readbackBuffer.unmap();

	// クリア
	u32 clearValues[4] = {};
	rhi::GpuDescriptorHandle gpuDescriptor = frameResource->_cullingResultUav._gpuHandle;
	rhi::CpuDescriptorHandle cpuDescriptor = frameResource->_cullingResultCpuUav._cpuHandle;
	rhi::Resource* resource = frameResource->_cullingResultGpuBuffer->getResource();
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

	// ビューフラスタムの平面情報
	Matrix4 cameraMatrix = camera->_worldMatrix;
	Vector3 sideForward = Vector3::Forward() * fovHalfTan * aspectRate;
	Vector3 forward = Vector3::Forward() * fovHalfTan;
	Vector3 rightNormal = Matrix4::transformNormal(Vector3::Right() + sideForward, cameraMatrix).normalize();
	Vector3 leftNormal = Matrix4::transformNormal(-Vector3::Right() + sideForward, cameraMatrix).normalize();
	Vector3 buttomNormal = Matrix4::transformNormal(Vector3::Up() + forward, cameraMatrix).normalize();
	Vector3 topNormal = Matrix4::transformNormal(-Vector3::Up() + forward, cameraMatrix).normalize();
	Vector3 nearNormal = Matrix4::transformNormal(Vector3::Forward(), cameraMatrix).normalize();
	Vector3 farNormal = Matrix4::transformNormal(-Vector3::Forward(), cameraMatrix).normalize();
	gpuView->_frustumPlanes[0] = MakeFloat4(rightNormal, Vector3::Dot(rightNormal, cameraPosition));
	gpuView->_frustumPlanes[1] = MakeFloat4(leftNormal, Vector3::Dot(leftNormal, cameraPosition));
	gpuView->_frustumPlanes[2] = MakeFloat4(buttomNormal, Vector3::Dot(buttomNormal, cameraPosition));
	gpuView->_frustumPlanes[3] = MakeFloat4(topNormal, Vector3::Dot(topNormal, cameraPosition));
	gpuView->_frustumPlanes[4] = MakeFloat4(nearNormal, Vector3::Dot(nearNormal, cameraPosition) + nearClip);
	gpuView->_frustumPlanes[5] = MakeFloat4(farNormal, Vector3::Dot(farNormal, cameraPosition) - farClip);
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

void RenderViewFrameResource::setUpFrameResource(const View* view, rhi::CommandList* commandList) {
	FrameBufferAllocator* bufferAllocator = FrameBufferAllocator::Get();
	// カラーテクスチャ
	{
		rhi::ClearValue optimizedClearValue = {};
		optimizedClearValue._format = rhi::BACK_BUFFER_FORMAT;

		FrameBufferAllocator::TextureCreatationDesc desc;
		desc._format = rhi::BACK_BUFFER_FORMAT;
		desc._optimizedClearValue = &optimizedClearValue;
		desc._width = view->getWidth();
		desc._height = view->getHeight();
		desc._initialState = rhi::RESOURCE_STATE_RENDER_TARGET;
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		_viewColorTexture = bufferAllocator->createGpuTexture(desc);
		_viewColorTexture->setName("ViewColor");
	}

	// デプステクスチャ
	{
		rhi::ClearValue depthOptimizedClearValue = {};
		depthOptimizedClearValue._format = rhi::FORMAT_D32_FLOAT;
		depthOptimizedClearValue._depthStencil._depth = 1.0f;
		depthOptimizedClearValue._depthStencil._stencil = 0;

		FrameBufferAllocator::TextureCreatationDesc desc;
		desc._format = rhi::FORMAT_D32_FLOAT;
		desc._optimizedClearValue = &depthOptimizedClearValue;
		desc._width = view->getWidth();
		desc._height = view->getHeight();
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		desc._initialState = rhi::RESOURCE_STATE_DEPTH_WRITE;
		_viewDepthTexture = bufferAllocator->createGpuTexture(desc);
		_viewDepthTexture->setName("ViewDepth");
	}

	// カリング結果バッファ
	{
		FrameBufferAllocator* frameBufferAllocator = FrameBufferAllocator::Get();
		FrameBufferAllocator::BufferCreatationDesc desc = {};
		desc._initialState = rhi::RESOURCE_STATE_UNORDERED_ACCESS;
		desc._sizeInByte = sizeof(gpu::CullingResult);
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		_cullingResultGpuBuffer = frameBufferAllocator->createGpuBuffer(desc);
		_cullingResultGpuBuffer->setName("ViewCullingResult");
	}

	// デスクリプター
	{
		FrameDescriptorAllocator* descriptorAllocator = FrameDescriptorAllocator::Get();
		_viewRtv = descriptorAllocator->allocateRtvGpu();
		_viewDsv = descriptorAllocator->allocateDsvGpu();
		_viewSrv = descriptorAllocator->allocateSrvCbvUavGpu();
		_viewDepthSrv = descriptorAllocator->allocateSrvCbvUavGpu();
		_cullingResultUav = descriptorAllocator->allocateSrvCbvUavGpu();
		_cullingResultCpuUav = descriptorAllocator->allocateSrvCbvUavCpu();

		rhi::Device* device = DeviceManager::Get()->getDevice();
		device->createDepthStencilView(_viewDepthTexture->getResource(), _viewDsv._cpuHandle);
		device->createRenderTargetView(_viewColorTexture->getResource(), _viewRtv._cpuHandle);
		device->createShaderResourceView(_viewColorTexture->getResource(), nullptr, _viewSrv._cpuHandle);

		rhi::ShaderResourceViewDesc srvDesc = {};
		srvDesc._format = rhi::FORMAT_R32_FLOAT;
		srvDesc._viewDimension = rhi::SRV_DIMENSION_TEXTURE2D;
		srvDesc._texture2D._mipLevels = 1;
		device->createShaderResourceView(_viewDepthTexture->getResource(), &srvDesc, _viewDepthSrv._cpuHandle);

		rhi::UnorderedAccessViewDesc uavDesc = {};
		uavDesc._viewDimension = rhi::UAV_DIMENSION_BUFFER;
		uavDesc._format = rhi::FORMAT_R32_TYPELESS;
		uavDesc._buffer._flags = rhi::BUFFER_UAV_FLAG_RAW;
		uavDesc._buffer._numElements = _cullingResultGpuBuffer->getU32ElementCount();
		device->createUnorderedAccessView(_cullingResultGpuBuffer->getResource(), nullptr, &uavDesc, _cullingResultUav._cpuHandle);
		device->createUnorderedAccessView(_cullingResultGpuBuffer->getResource(), nullptr, &uavDesc, _cullingResultCpuUav._cpuHandle);
	}
}
}
