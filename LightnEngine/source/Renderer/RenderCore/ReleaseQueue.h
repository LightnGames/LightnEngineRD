#pragma once
#include <Core/Utility.h>
#include <Renderer/RHI/Rhi.h>

namespace ltn {
class FrameQueue {
public:
	static constexpr u32 RESOURCE_COUNT_MAX = 128;
	void enqueue(rhi::Resource& resource);
	void enqueue(rhi::PipelineState& pipelineState);
	void execureRelease();

private:
	u32 _resourceCount = 0;
	u32 _pipelineStateCount = 0;
	rhi::Resource _resources[RESOURCE_COUNT_MAX] = {};
	rhi::PipelineState _pipelineStates[RESOURCE_COUNT_MAX] = {};
};

class ReleaseQueue {
public:
	void update();
	void enqueue(rhi::Resource& resource);
	void enqueue(rhi::PipelineState& pipelineState);

	static ReleaseQueue* Get();

private:
	FrameQueue* getCurrentFrameQueue();

private:
	u32 _currentFrameQueueIndex = 0;
	FrameQueue _frameQueues[rhi::BACK_BUFFER_COUNT] = {};
};
}