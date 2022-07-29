#include "GpuTexture.h"

namespace ltn {
void GpuTexture::initializeFromBackbuffer(rhi::SwapChain* swapChain, u32 backBufferIndex) {
	swapChain->getBackBuffer(_resource, backBufferIndex);
	_currentState = rhi::RESOURCE_STATE_PRESENT;
}

void GpuTexture::initialize(const GpuTextureDesc& desc) {
	rhi::ResourceDesc textureDesc = {};
	textureDesc._format = desc._format;
	textureDesc._dimension = desc._dimension;
	textureDesc._width = desc._width;
	textureDesc._height = desc._height;
	textureDesc._depthOrArraySize = desc._depthOrArraySize;
	textureDesc._mipLevels = desc._mipLevels;
	textureDesc._sampleDesc._count = desc._sampleCount;
	textureDesc._sampleDesc._quality = desc._sampleQuality;
	textureDesc._flags = desc._flags;
	_currentState = desc._initialState;
	_desc = textureDesc;

	if (desc._aliasingMemoryAllocation != nullptr) {
		LTN_ASSERT(desc._allocator != nullptr);
		desc._allocator->createAliasingResource(textureDesc, desc._offsetSizeInByte, desc._initialState, desc._optimizedClearValue, desc._aliasingMemoryAllocation, &_resource);
		return;
	}

	if (desc._allocator != nullptr) {
		desc._allocator->createResource(textureDesc, desc._initialState, desc._optimizedClearValue, &_allocation, &_resource);
		return;
	}

	desc._device->createCommittedResource(rhi::HEAP_TYPE_DEFAULT, rhi::HEAP_FLAG_NONE, textureDesc, desc._initialState, desc._optimizedClearValue, &_resource);
}
}
