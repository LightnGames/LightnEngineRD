#pragma once
#include <Renderer/RenderCore/GpuResource.h>

namespace ltn {
struct GpuTextureDesc {
	// アロケーターへのポインタを有効にするとGPUリソースはロケーターから確保されます
	rhi::VideoMemoryAllocator* _allocator = nullptr;
	rhi::Device* _device = nullptr;
	rhi::ResourceStates _initialState;
	rhi::ClearValue* _optimizedClearValue = nullptr;
	rhi::Format _format = rhi::FORMAT_UNKNOWN;
	rhi::ResourceDimension _dimension = rhi::RESOURCE_DIMENSION_TEXTURE2D;
	u64 _width = 0;
	u32 _height = 0;
	u16 _depthOrArraySize = 1;
	u16 _mipLevels = 1;
	u32 _sampleCount = 1;
	u32 _sampleQuality = 0;
	rhi::ResourceFlags _flags = rhi::RESOURCE_FLAG_NONE;
};

class GpuTexture :public GpuResource {
public:
	void initializeFromBackbuffer(rhi::SwapChain* swapChain, u32 backBufferIndex);
	void initialize(const GpuTextureDesc& desc);
};
}