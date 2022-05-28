#include "RenderView.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <RendererScene/View.h>

namespace ltn {
namespace {
RenderViewScene g_renderViewScene;
}
void RenderViewScene::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	u32 alignedViewSize = rhi::GetConstantBufferAligned(sizeof(gpu::View));

	// GPU リソース
	GpuBufferDesc desc = {};
	desc._device = device;
	desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	desc._sizeInByte = ViewScene::VIEW_COUNT_MAX * alignedViewSize;
	_viewConstantBuffer.initialize(desc);

	// コンスタントバッファービュー
	_viewDescriptors = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	for (u32 i = 0; i < ViewScene::VIEW_COUNT_MAX; ++i) {
		rhi::ConstantBufferViewDesc desc;
		desc._bufferLocation = _viewConstantBuffer.getGpuVirtualAddress() + alignedViewSize * i;
		desc._sizeInBytes = alignedViewSize;
		device->createConstantBufferView(desc, _viewDescriptors._firstHandle._cpuHandle + _viewDescriptors._incrementSize * i);
	}
}

void RenderViewScene::terminate() {
	_viewConstantBuffer.terminate();
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_viewDescriptors);
}

void RenderViewScene::update() {
	ViewScene* viewScene = ViewScene::Get();
	const View* views = viewScene->getView(0);
	const u8* updateFlags = viewScene->getViewUpdateFlags();
	for (u32 i = 0; i < ViewScene::VIEW_COUNT_MAX; ++i) {
		if (updateFlags[i] == 0) {
			continue;
		}

		const View& view = views[i];
		f32 farClip = 300;
		f32 nearClip = 0.1f;
		f32 fov = 1.0472f;
		f32 fovHalfTan = tanf(fov / 2.0f);
		f32 aspectRate = 1.0f;
		Vector3 cameraPosition(0, 0, 0);
		Matrix4 cameraRotateMatrix;
		Matrix4 cameraWorldMatrix = cameraRotateMatrix * Matrix4::translationFromVector(cameraPosition);
		Matrix4 viewMatrix;
		Matrix4 projectionMatrix = Matrix4::perspectiveFovLH(fov, aspectRate, nearClip, farClip);
		Matrix4 viewProjectionMatrix = projectionMatrix * viewMatrix;
		gpu::View* gpuView = VramUpdater::Get()->enqueueUpdate<gpu::View>(&_viewConstantBuffer);
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
		gpuView->_upDirection = cameraRotateMatrix.getCol(1).getFloat3();
	}
}

RenderViewScene* RenderViewScene::Get() {
	return &g_renderViewScene;
}
}
