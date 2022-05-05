#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuTexture.h>
#include <Renderer/RenderCore/CommandListPool.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/ImGuiSystem.h>

namespace ltn {
class Renderer {
public:
	void initialize();
	void terminate();
	void update();
	void render();

	ImGuiSystem* getImGuiSystem() { return &_imguiSystem; }

	static Renderer* Get();

private:
	void moveToNextFrame();

private:
	rhi::Device _device;
	rhi::HardwareFactory _factory;
	rhi::HardwareAdapter _adapter;
	rhi::SwapChain _swapChain;
	rhi::CommandQueue _commandQueue;

	CommandListPool _commandListPool;
	DescriptorAllocatorGroup _descriptorAllocatorGroup;
	ImGuiSystem _imguiSystem;

	u64 _fenceValues[rhi::BACK_BUFFER_COUNT] = {};
	GpuTexture _backBuffers[rhi::BACK_BUFFER_COUNT] = {};
	DescriptorHandles _rtvDescriptors;
	u32 _frameIndex = 0;
};
}