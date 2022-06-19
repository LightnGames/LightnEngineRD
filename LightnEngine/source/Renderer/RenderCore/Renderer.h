#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuTexture.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/CommandListPool.h>

namespace ltn {
class Renderer {
public:
	void initialize();
	void terminate();
	void update();
	void render();
	void waitForIdle();

	static Renderer* Get();

private:
	void moveToNextFrame();

private:
	rhi::SwapChain _swapChain;
	rhi::CommandQueue _commandQueue;

	u32 _frameIndex = 0;
	u64 _fenceValues[rhi::BACK_BUFFER_COUNT] = {};
	GpuTexture _backBuffers[rhi::BACK_BUFFER_COUNT] = {};
	CommandListPool _commandListPool;
};
}