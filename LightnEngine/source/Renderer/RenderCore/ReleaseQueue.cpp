#include "ReleaseQueue.h"

namespace ltn {
namespace {
ReleaseQueue g_releaseQueue;
}
void ReleaseQueue::update() {
	_frameQueues[_currentFrameQueueIndex].execureRelease();
	_currentFrameQueueIndex = (_currentFrameQueueIndex + 1) % rhi::BACK_BUFFER_COUNT;
}

void ReleaseQueue::enqueue(rhi::Resource& resource) {
	getCurrentFrameQueue()->enqueue(resource);
}

void ReleaseQueue::enqueue(rhi::PipelineState& pipelineState) {
	getCurrentFrameQueue()->enqueue(pipelineState);
}

ReleaseQueue* ReleaseQueue::Get() {
	return &g_releaseQueue;
}

FrameQueue* ReleaseQueue::getCurrentFrameQueue() {
	u32 enqueueFrameIndex = (_currentFrameQueueIndex - 1) % rhi::BACK_BUFFER_COUNT;
	return &_frameQueues[enqueueFrameIndex];
}

void FrameQueue::enqueue(rhi::Resource& resource) {
	_resources[_resourceCount++] = resource;
}

void FrameQueue::enqueue(rhi::PipelineState& pipelineState) {
	_pipelineStates[_pipelineStateCount++] = pipelineState;
}

void FrameQueue::execureRelease() {
	for (u32 i = 0; i < _resourceCount; ++i) {
		_resources[i].terminate();
	}

	for (u32 i = 0; i < _pipelineStateCount; ++i) {
		_pipelineStates[i].terminate();
	}

	_resourceCount = 0;
	_pipelineStateCount = 0;
}
}