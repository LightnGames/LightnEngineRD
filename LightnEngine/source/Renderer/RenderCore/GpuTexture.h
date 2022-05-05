#pragma once
#include <Renderer/RenderCore/GpuResource.h>

namespace ltn {
class GpuTexture :public GpuResource {
public:
	void initializeFromBackbuffer(rhi::SwapChain* swapChain, u32 backBufferIndex);
};
}