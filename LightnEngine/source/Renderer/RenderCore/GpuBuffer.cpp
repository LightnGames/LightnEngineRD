#include "GpuBuffer.h"
#include <ThiredParty/D3D12MemoryAllocator/D3D12MemAlloc.h>

namespace ltn {
void GpuBuffer::initialize(const GpuBufferDesc& desc) {
	rhi::ResourceDesc bufferDesc = {};
	bufferDesc._dimension = rhi::RESOURCE_DIMENSION_BUFFER;
	bufferDesc._width = desc._sizeInByte;
	bufferDesc._height = 1;
	bufferDesc._depthOrArraySize = 1;
	bufferDesc._mipLevels = 1;
	bufferDesc._sampleDesc._count = 1;
	bufferDesc._flags = desc._flags;
	bufferDesc._layout = rhi::TEXTURE_LAYOUT_ROW_MAJOR;
	_desc = bufferDesc;
	_currentState = desc._initialState;

	if (desc._allocator != nullptr) {
		desc._allocator->createResource(bufferDesc, desc._initialState, &_allocation, &_resource);
		return;
	}
	desc._device->createCommittedResource(desc._heapType, rhi::HEAP_FLAG_NONE, bufferDesc, desc._initialState, nullptr, &_resource);
}

void GpuBuffer::initialize(const GpuDynamicBufferDesc& desc) {
	rhi::ResourceDesc bufferDesc = {};
	bufferDesc._dimension = rhi::RESOURCE_DIMENSION_BUFFER;
	bufferDesc._width = desc._sizeInByte;
	bufferDesc._height = 1;
	bufferDesc._depthOrArraySize = 1;
	bufferDesc._mipLevels = 1;
	bufferDesc._sampleDesc._count = 1;
	bufferDesc._layout = rhi::TEXTURE_LAYOUT_ROW_MAJOR;
	desc._device->createCommittedResource(rhi::HEAP_TYPE_UPLOAD, rhi::HEAP_FLAG_NONE, bufferDesc, rhi::RESOURCE_STATE_GENERIC_READ, nullptr, &_resource);
	_desc = bufferDesc;
	_currentState = rhi::RESOURCE_STATE_GENERIC_READ;
}
}