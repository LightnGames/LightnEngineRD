#pragma once

#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/GpuTexture.h>

namespace ltn {
class FrameBufferAllocator {
public:
	struct BufferCreatationDesc {
		u32 _sizeInByte = 0;
		rhi::ResourceFlags _flags = rhi::RESOURCE_FLAG_NONE;
		rhi::ResourceStates _initialState = rhi::RESOURCE_STATE_COMMON;
	};

	void initialize();
	void terminate();

	void reset();

	GpuBuffer* createGpuBuffer(const BufferCreatationDesc& desc);

	//GpuTexture* createGpuTexture() {
	//	GpuTexture* gpuTexture = &_gpuTextures[_allocationCount];
	//	GpuTextureDesc textureDesc = {};
	//	textureDesc._device = DeviceManager::Get()->getDevice();
	//	textureDesc._format = format;
	//	textureDesc._width = width;
	//	textureDesc._height = height;
	//	textureDesc._mipLevels = mipLevel;
	//	textureDesc._initialState = rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	//	textureDesc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	//	gpuTexture->initialize(textureDesc);
	//	return gpuTexture;
	//}

	static FrameBufferAllocator* Get();

private:
	static constexpr u32 ALLOCATION_CAPACITY = 128;
	static constexpr u32 ALLOCATION_SIZE_IN_BYTE = rhi::DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 128;
	GpuBuffer _gpuBuffers[ALLOCATION_CAPACITY] = {};
	GpuTexture _gpuTextures[ALLOCATION_CAPACITY] = {};
	u32 _allocationCount = 0;
	u32 _allocationOffset = 0;
	GpuBuffer _defaultGpuBuffer;
	GpuAliasingMemoryAllocation _aliasingAllocation;
};

class FrameDescriptorAllocator {
public:
	static constexpr u32 ALLOCATION_CAPACITY = 256;

	void initialize();
	void terminate();

	DescriptorHandles allocateSrvCbvUavGpu(u32 count);
	DescriptorHandle allocateSrvCbvUavGpu();

	DescriptorHandles allocateSrvCbvUavCpu(u32 count);
	DescriptorHandle allocateSrvCbvUavCpu();

	void reset();

	static FrameDescriptorAllocator* Get();

private:
	u32 _frameIndex = 0;
	u32 _srvCbvUavGpuAllocationOffset = 0;
	u32 _srvCbvUavCpuAllocationOffset = 0;
	DescriptorHandles _srvCbvUavGpuDescriptors[rhi::BACK_BUFFER_COUNT] = {};
	DescriptorHandles _srvCbvUavCpuDescriptors[rhi::BACK_BUFFER_COUNT] = {};
};
}