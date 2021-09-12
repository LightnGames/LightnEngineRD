#include <TextureSystem/impl/TextureStreamingSystem.h>
#include <TextureSystem/impl/TextureSystemImpl.h>
#include <Core/Application.h>

TextureStreamingSystem _textureStreamingSystem;
void TextureStreamingSystem::initialize() {}

void TextureStreamingSystem::terminate() {}

void TextureStreamingSystem::update() {
	for (u32 reloadIndex = 0; reloadIndex < TextureSystem::RELOAD_QUEUE_COUNT_MAX; ++reloadIndex) {
		if (_reloadQueueTimers[reloadIndex] == 0) {
			continue;
		}

		u32 lifeTime = --_reloadQueueTimers[reloadIndex];
		if (lifeTime == 0) {
			_reloadTextures[reloadIndex]->terminate();
			_reloadQueueCount--;
		}
	}

	TextureSystemImpl* textureSystem = TextureSystemImpl::Get();
	u32 textureCount = textureSystem->getTextureResarveCount();
	for (u32 textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
		if (textureSystem->getAssetStateFlags()[textureIndex] != ASSET_STATE_ENABLE) {
			continue;
		}
		u32 targetStreamingLevel = _targetStreamingLevels[textureIndex];
		if (_currentStreamingLevels[textureIndex] == targetStreamingLevel) {
			continue;
		}

		if (textureIndex == 0) {
			continue;
		}

		TextureImpl* texture = textureSystem->getTexture(textureIndex);
		u32 reloadQueueIndex = (_reloadQueueOffset++ % TextureSystem::RELOAD_QUEUE_COUNT_MAX);
		_reloadQueueCount++;
		_reloadQueueTimers[reloadQueueIndex] = BACK_BUFFER_COUNT;
		_reloadTextures[reloadQueueIndex] = texture->getGpuTexture()->getResource();

		u32 targetResolution = max(64, 1 << targetStreamingLevel);
		textureSystem->loadTexture(textureIndex, targetResolution);

		_currentStreamingLevels[textureIndex] = targetStreamingLevel;
	}
}

void TextureStreamingSystem::updateStreamingLevel(u32 textureIndex, f32 screenArea) {
	TextureImpl* texture = TextureSystemImpl::Get()->getTexture(textureIndex);
	Application* app = ApplicationSystem::Get()->getApplication();
	u32 screenWidth = app->getScreenWidth();
	u64 sourceWidth = texture->getGpuTexture()->getResourceDesc()._width;
	u32 targetWidth = screenArea * screenWidth;
	u8 requestMipLevel = 0;
	for (u32 i = 1; i < targetWidth; i *= 2) {
		requestMipLevel++;
	}
	_targetStreamingLevels[textureIndex] = min(_targetStreamingLevels[textureIndex], requestMipLevel);
}

void TextureStreamingSystem::resetStreamingLevels() {
	memset(_targetStreamingLevels, 0xff, sizeof(u8) * LTN_COUNTOF(_targetStreamingLevels));
}

TextureStreamingSystem* TextureStreamingSystem::Get()
{
	return &_textureStreamingSystem;
}
