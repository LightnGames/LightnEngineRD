#pragma once
#include <TextureSystem/TextureSystem.h>
#include <GfxCore/impl/GpuResourceImpl.h>

class LTN_TEXTURE_SYSTEM_API TextureStreamingSystem {
public:
	void initialize();
	void terminate();
	void update();
	void updateStreamingLevel(u32 textureIndex, f32 screenArea);

	static TextureStreamingSystem* Get();

private:
	u8 _currentStreamingLevels[TextureSystem::TEXTURE_COUNT_MAX] = {};
	u8 _targetStreamingLevels[TextureSystem::TEXTURE_COUNT_MAX] = {};
};