#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuTexture.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
class Renderer {
public:
	void initialize();
	void terminate();
	void update();
	void render();

	static Renderer* Get();

private:
	void moveToNextFrame();

private:
	rhi::SwapChain _swapChain;
	rhi::CommandQueue _commandQueue;

	u64 _fenceValues[rhi::BACK_BUFFER_COUNT] = {};
	GpuTexture _backBuffers[rhi::BACK_BUFFER_COUNT] = {};
	DescriptorHandles _rtvDescriptors;
	u32 _frameIndex = 0;
};
}