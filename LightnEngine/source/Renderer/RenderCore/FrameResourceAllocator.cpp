#include "FrameResourceAllocator.h"
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/RenderCore/DeviceManager.h>

namespace ltn {
namespace {
FrameBufferAllocator g_frameBufferAllocator;
FrameDescriptorAllocator g_frameDescriptorAllocator;
}

void FrameBufferAllocator::initialize() {
	_aliasingAllocation.initialize(GlobalVideoMemoryAllocator::Get()->getAllocator(), ALLOCATION_SIZE_IN_BYTE);

	GpuBufferDesc desc = {};
	desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	desc._aliasingMemoryAllocation = &_aliasingAllocation;
	desc._device = DeviceManager::Get()->getDevice();
	desc._sizeInByte = ALLOCATION_SIZE_IN_BYTE;
	desc._initialState = rhi::RESOURCE_STATE_COMMON;
	_defaultGpuBuffer.initialize(desc);
	_defaultGpuBuffer.setName("FrameAliasingResource");
}

void FrameBufferAllocator::terminate() {
	_defaultGpuBuffer.terminate();
	_aliasingAllocation.terminate();
}

void FrameBufferAllocator::reset() {
	ReleaseQueue* releaseQueue = ReleaseQueue::Get();
	for (u32 i = 0; i < _allocationCount; ++i) {
		releaseQueue->enqueue(_gpuBuffers[i]);
	}

	_allocationOffset = 0;
	_allocationCount = 0;
}

GpuBuffer* FrameBufferAllocator::createGpuBuffer(const BufferCreatationDesc& desc) {
	GpuBuffer* gpuBuffer = &_gpuBuffers[_allocationCount];
	GpuBufferDesc bufferDesc = {};
	bufferDesc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	bufferDesc._aliasingMemoryAllocation = &_aliasingAllocation;
	bufferDesc._device = DeviceManager::Get()->getDevice();
	bufferDesc._sizeInByte = GetAligned(desc._sizeInByte, rhi::DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	bufferDesc._initialState = desc._initialState;
	bufferDesc._flags = desc._flags;
	gpuBuffer->initialize(bufferDesc);

	_allocationOffset += bufferDesc._sizeInByte;
	_allocationCount++;

	return gpuBuffer;
}

FrameBufferAllocator* FrameBufferAllocator::Get() {
	return &g_frameBufferAllocator;
}

void FrameDescriptorAllocator::initialize() {
	DescriptorAllocatorGroup* allocatorGroup = DescriptorAllocatorGroup::Get();
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		_srvCbvUavGpuDescriptors[i] = allocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ALLOCATION_CAPACITY);
		_srvCbvUavCpuDescriptors[i] = allocatorGroup->getSrvCbvUavCpuAllocator()->allocate(ALLOCATION_CAPACITY);
	}
}

void FrameDescriptorAllocator::terminate() {
	DescriptorAllocatorGroup* allocatorGroup = DescriptorAllocatorGroup::Get();
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		allocatorGroup->getSrvCbvUavGpuAllocator()->free(_srvCbvUavGpuDescriptors[i]);
		allocatorGroup->getSrvCbvUavCpuAllocator()->free(_srvCbvUavCpuDescriptors[i]);
	}
}

DescriptorHandles FrameDescriptorAllocator::allocateSrvCbvUavGpu(u32 count) {
	DescriptorHandles result;
	result._descriptorCount = count;
	result._firstHandle = _srvCbvUavGpuDescriptors[_frameIndex].get(_srvCbvUavGpuAllocationOffset);
	result._incrementSize = _srvCbvUavGpuDescriptors[_frameIndex]._incrementSize;
	_srvCbvUavGpuAllocationOffset += count;
	return result;
}

DescriptorHandle FrameDescriptorAllocator::allocateSrvCbvUavGpu() {
	return _srvCbvUavGpuDescriptors[_frameIndex].get(_srvCbvUavGpuAllocationOffset++);
}

DescriptorHandles FrameDescriptorAllocator::allocateSrvCbvUavCpu(u32 count) {
	DescriptorHandles result;
	result._descriptorCount = count;
	result._firstHandle = _srvCbvUavCpuDescriptors[_frameIndex].get(_srvCbvUavCpuAllocationOffset);
	result._incrementSize = _srvCbvUavCpuDescriptors[_frameIndex]._incrementSize;
	_srvCbvUavCpuAllocationOffset += count;
	return result;
}

DescriptorHandle FrameDescriptorAllocator::allocateSrvCbvUavCpu() {
	return _srvCbvUavCpuDescriptors[_frameIndex].get(_srvCbvUavCpuAllocationOffset++);
}

void FrameDescriptorAllocator::reset() {
	_srvCbvUavGpuAllocationOffset = 0;
	_srvCbvUavCpuAllocationOffset = 0;
	_frameIndex = (_frameIndex + 1) % rhi::BACK_BUFFER_COUNT;
}

FrameDescriptorAllocator* FrameDescriptorAllocator::Get() {
	return &g_frameDescriptorAllocator;
}
}
