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
	const u8* updateFlags = viewScene->getViewUpdateFlags();
	for (u32 i = 0; i < ViewScene::VIEW_COUNT_MAX; ++i) {
		if (updateFlags[i] == 0) {
			continue;
		}

		gpu::View* gpuView = VramUpdater::Get()->enqueueUpdate<gpu::View>(&_viewConstantBuffer);
		gpuView->_viewMatrix;
	}
}

RenderViewScene* RenderViewScene::Get() {
	return &g_renderViewScene;
}
}
