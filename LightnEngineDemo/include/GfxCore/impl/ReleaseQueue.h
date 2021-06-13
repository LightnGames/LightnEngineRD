#pragma once
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
struct Resource;

class LTN_GFX_CORE_API FrameQueue {
public:
	static constexpr u32 RESOURCE_COUNT_MAX = 128;
	void enqueueResource(Resource* resource);
	void execureRelease();

private:
	u32 _resourceCount = 0;
	Resource* _resources[RESOURCE_COUNT_MAX] = {};
};

class LTN_GFX_CORE_API ReleaseQueue {
public:
	void enqueueResource(Resource* resource);
	void execureRelease();

private:
	u32 _currentFrameQueueIndex = 0;
	FrameQueue _frameQueues[BACK_BUFFER_COUNT] = {};
};