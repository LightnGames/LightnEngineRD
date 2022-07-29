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

	struct TextureCreatationDesc {
		u64 _width = 0;
		u32 _height = 0;
		rhi::ResourceStates _initialState = rhi::RESOURCE_STATE_COMMON;
		rhi::ClearValue* _optimizedClearValue = nullptr;
		rhi::Format _format = rhi::FORMAT_UNKNOWN;
		rhi::ResourceFlags _flags = rhi::RESOURCE_FLAG_NONE;
	};

	void initialize();
	void terminate();

	void reset();

	GpuBuffer* createGpuBuffer(const BufferCreatationDesc& desc);
	GpuTexture* createGpuTexture(const TextureCreatationDesc& desc);

	static FrameBufferAllocator* Get();

private:
	static constexpr u32 ALLOCATION_CAPACITY = 128;
	static constexpr u32 ALLOCATION_SIZE_IN_BYTE = rhi::DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 1024;
	GpuBuffer _gpuBuffers[ALLOCATION_CAPACITY] = {};
	GpuTexture _gpuTextures[ALLOCATION_CAPACITY] = {};
	u32 _allocationBufferCount = 0;
	u32 _allocationTextureCount = 0;
	u32 _allocationOffset = 0;
	GpuAliasingMemoryAllocation _aliasingAllocation;
};

class FrameDescriptorAllocator {
public:
	static constexpr u32 ALLOCATION_CAPACITY = 256;

	void initialize();
	void terminate();

	DescriptorHandles allocateSrvCbvUavGpu(u32 count);
	DescriptorHandle allocateSrvCbvUavGpu();

	DescriptorHandles allocateRtvGpu(u32 count);
	DescriptorHandle allocateRtvGpu();

	DescriptorHandles allocateDsvGpu(u32 count);
	DescriptorHandle allocateDsvGpu();

	DescriptorHandles allocateSrvCbvUavCpu(u32 count);
	DescriptorHandle allocateSrvCbvUavCpu();

	void reset();

	static FrameDescriptorAllocator* Get();

private:
	u32 _frameIndex = 0;
	u32 _rtvAllocationOffset = 0;
	u32 _dsvAllocationOffset = 0;
	u32 _srvCbvUavGpuAllocationOffset = 0;
	u32 _srvCbvUavCpuAllocationOffset = 0;
	DescriptorHandles _rtvGpuDescriptors[rhi::BACK_BUFFER_COUNT] = {};
	DescriptorHandles _dsvGpuDescriptors[rhi::BACK_BUFFER_COUNT] = {};
	DescriptorHandles _srvCbvUavGpuDescriptors[rhi::BACK_BUFFER_COUNT] = {};
	DescriptorHandles _srvCbvUavCpuDescriptors[rhi::BACK_BUFFER_COUNT] = {};
};
}