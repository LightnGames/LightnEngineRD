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
}

void FrameBufferAllocator::terminate() {
	_aliasingAllocation.terminate();
}

void FrameBufferAllocator::reset() {
	ReleaseQueue* releaseQueue = ReleaseQueue::Get();
	for (u32 i = 0; i < _allocationBufferCount; ++i) {
		releaseQueue->enqueue(_gpuBuffers[i]);
	}

	for (u32 i = 0; i < _allocationTextureCount; ++i) {
		releaseQueue->enqueue(_gpuTextures[i]);
	}

	_allocationOffset = 0;
	_allocationBufferCount = 0;
	_allocationTextureCount = 0;
}

GpuBuffer* FrameBufferAllocator::createGpuBuffer(const BufferCreatationDesc& desc) {
	GpuBuffer* gpuBuffer = &_gpuBuffers[_allocationBufferCount];
	GpuBufferDesc bufferDesc = {};
	bufferDesc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	bufferDesc._aliasingMemoryAllocation = _aliasingAllocation.getAllocation();
	bufferDesc._device = DeviceManager::Get()->getDevice();
	bufferDesc._sizeInByte = GetAligned(desc._sizeInByte, rhi::DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	bufferDesc._initialState = desc._initialState;
	bufferDesc._flags = desc._flags;
	bufferDesc._offsetSizeInByte = _allocationOffset;
	gpuBuffer->initialize(bufferDesc);

	_allocationOffset += bufferDesc._sizeInByte;
	_allocationBufferCount++;

	return gpuBuffer;
}

GpuTexture* FrameBufferAllocator::createGpuTexture(const TextureCreatationDesc& desc) {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	GpuTexture* gpuTexture = &_gpuTextures[_allocationTextureCount];
	GpuTextureDesc textureDesc = {};
	textureDesc._device = device;
	textureDesc._format = desc._format;
	textureDesc._width = desc._width;
	textureDesc._height = desc._height;
	textureDesc._initialState = desc._initialState;
	textureDesc._optimizedClearValue = desc._optimizedClearValue;
	textureDesc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	textureDesc._aliasingMemoryAllocation = _aliasingAllocation.getAllocation();
	textureDesc._offsetSizeInByte = _allocationOffset;
	textureDesc._flags = desc._flags;
	gpuTexture->initialize(textureDesc);

	_allocationOffset += u32(device->getResourceAllocationInfo(0, 1, &gpuTexture->getResourceDesc())._sizeInBytes);
	_allocationTextureCount++;

	return gpuTexture;
}

FrameBufferAllocator* FrameBufferAllocator::Get() {
	return &g_frameBufferAllocator;
}

void FrameDescriptorAllocator::initialize() {
	DescriptorAllocatorGroup* allocatorGroup = DescriptorAllocatorGroup::Get();
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		_srvCbvUavGpuDescriptors[i] = allocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ALLOCATION_CAPACITY);
		_srvCbvUavCpuDescriptors[i] = allocatorGroup->getSrvCbvUavCpuAllocator()->allocate(ALLOCATION_CAPACITY);
		_rtvGpuDescriptors[i] = allocatorGroup->getRtvAllocator()->allocate(ALLOCATION_CAPACITY);
		_dsvGpuDescriptors[i] = allocatorGroup->getDsvAllocator()->allocate(ALLOCATION_CAPACITY);
	}
}

void FrameDescriptorAllocator::terminate() {
	DescriptorAllocatorGroup* allocatorGroup = DescriptorAllocatorGroup::Get();
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		allocatorGroup->getSrvCbvUavGpuAllocator()->free(_srvCbvUavGpuDescriptors[i]);
		allocatorGroup->getSrvCbvUavCpuAllocator()->free(_srvCbvUavCpuDescriptors[i]);
		allocatorGroup->getRtvAllocator()->free(_rtvGpuDescriptors[i]);
		allocatorGroup->getDsvAllocator()->free(_dsvGpuDescriptors[i]);
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

DescriptorHandles FrameDescriptorAllocator::allocateRtvGpu(u32 count) {
	DescriptorHandles result;
	result._descriptorCount = count;
	result._firstHandle = _rtvGpuDescriptors[_frameIndex].get(_rtvAllocationOffset);
	result._incrementSize = _rtvGpuDescriptors[_frameIndex]._incrementSize;
	_rtvAllocationOffset += count;
	return result;
}

DescriptorHandle FrameDescriptorAllocator::allocateRtvGpu() {
	return _rtvGpuDescriptors[_frameIndex].get(_rtvAllocationOffset++);
}

DescriptorHandles FrameDescriptorAllocator::allocateDsvGpu(u32 count) {
	DescriptorHandles result;
	result._descriptorCount = count;
	result._firstHandle = _dsvGpuDescriptors[_frameIndex].get(_dsvAllocationOffset);
	result._incrementSize = _dsvGpuDescriptors[_frameIndex]._incrementSize;
	_dsvAllocationOffset += count;
	return result;
}

DescriptorHandle FrameDescriptorAllocator::allocateDsvGpu() {
	return _dsvGpuDescriptors[_frameIndex].get(_dsvAllocationOffset++);
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
	_rtvAllocationOffset = 0;
	_dsvAllocationOffset = 0;
	_frameIndex = (_frameIndex + 1) % rhi::BACK_BUFFER_COUNT;
}

FrameDescriptorAllocator* FrameDescriptorAllocator::Get() {
	return &g_frameDescriptorAllocator;
}
}
