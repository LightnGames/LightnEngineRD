#pragma once
#include <TextureSystem/TextureSystem.h>
#include <GfxCore/impl/GpuResourceImpl.h>

class LTN_TEXTURE_SYSTEM_API TextureStreamingSystem {
public:
	void initialize();
	void terminate();
	void update();
	void updateStreamingLevel(u32 textureIndex, f32 screenArea);
	void resetStreamingLevels();

	static TextureStreamingSystem* Get();

private:
	u8 _currentStreamingLevels[TextureSystem::TEXTURE_COUNT_MAX] = {};
	u8 _targetStreamingLevels[TextureSystem::TEXTURE_COUNT_MAX] = {};

	u32 _reloadQueueCount = 0;
	u32 _reloadQueueOffset = 0;
	u32 _reloadQueueTimers[TextureSystem::RELOAD_QUEUE_COUNT_MAX] = {};
	Resource* _reloadTextures[TextureSystem::RELOAD_QUEUE_COUNT_MAX] = {};
	u32 _reloadTextureIndices[TextureSystem::RELOAD_QUEUE_COUNT_MAX] = {};
};