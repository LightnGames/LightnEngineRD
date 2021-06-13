#include <GfxCore/impl/ReleaseQueue.h>

void ReleaseQueue::enqueueResource(Resource* resource) {
	u32 enqueueFrameIndex = (_currentFrameQueueIndex - 1) % BACK_BUFFER_COUNT;
	_frameQueues[enqueueFrameIndex].enqueueResource(resource);
}

void ReleaseQueue::execureRelease() {
	_frameQueues[_currentFrameQueueIndex].execureRelease();
	_currentFrameQueueIndex = (_currentFrameQueueIndex + 1) % BACK_BUFFER_COUNT;
}

void FrameQueue::enqueueResource(Resource* resource) {
	_resources[_resourceCount++] = resource;
}

void FrameQueue::execureRelease() {
	for (u32 i = 0; i < _resourceCount; ++i) {
		_resources[i]->terminate();
	}
	_resourceCount = 0;
}
