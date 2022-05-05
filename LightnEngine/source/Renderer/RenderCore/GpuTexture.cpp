#include "GpuTexture.h"

namespace ltn {
void GpuTexture::initializeFromBackbuffer(rhi::SwapChain* swapChain, u32 backBufferIndex) {
	swapChain->getBackBuffer(_resource, backBufferIndex);
	_currentState = rhi::RESOURCE_STATE_PRESENT;
}
}
