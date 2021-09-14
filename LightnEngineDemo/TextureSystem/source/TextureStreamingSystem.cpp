#include <TextureSystem/impl/TextureStreamingSystem.h>
#include <TextureSystem/impl/TextureSystemImpl.h>
#include <Core/Application.h>

TextureStreamingSystem _textureStreamingSystem;
void TextureStreamingSystem::initialize() {}

void TextureStreamingSystem::terminate() {}

void TextureStreamingSystem::update() {
	TextureSystemImpl* textureSystem = TextureSystemImpl::Get();

	// 再生成要求で設定した寿命が来たら新しいテクスチャSRVに差し替えて古いリソースを破棄
	for (u32 reloadIndex = 0; reloadIndex < TextureSystem::RELOAD_QUEUE_COUNT_MAX; ++reloadIndex) {
		if (_reloadQueueTimers[reloadIndex] == 0) {
			continue;
		}

		u32 lifeTime = --_reloadQueueTimers[reloadIndex];
		if (lifeTime == 0) {
			textureSystem->initializeShaderResourceView(_reloadTextureIndices[reloadIndex]);
			_reloadTextures[reloadIndex]->terminate();
			_reloadQueueCount--;
		}
	}

	// 要求ストリーミングレベルが現在のレベルと違うテクスチャに対して再生成要求
	u32 textureCount = textureSystem->getTextureResarveCount();
	for (u32 textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
		if (textureSystem->getAssetStateFlags()[textureIndex] != ASSET_STATE_ENABLE) {
			continue;
		}
		u32 targetStreamingLevel = _targetStreamingLevels[textureIndex];
		if (targetStreamingLevel == 0xff) {
			continue;
		}

		u32 currentStreamingLevel = _currentStreamingLevels[textureIndex];
		if (currentStreamingLevel == targetStreamingLevel) {
			continue;
		}

		// コモンリソースはストリーミングしない
		// TODO: テクスチャごとにストリーミング設定を追加する
		if (textureIndex == 0) {
			continue;
		}

		TextureImpl* texture = textureSystem->getTexture(textureIndex);
		GpuTexture currentTexture = *texture->getGpuTexture();
		u32 reloadQueueIndex = (_reloadQueueOffset++ % TextureSystem::RELOAD_QUEUE_COUNT_MAX);
		_reloadQueueCount++;
		_reloadQueueTimers[reloadQueueIndex] = BACK_BUFFER_COUNT;
		_reloadTextures[reloadQueueIndex] = currentTexture.getResource();
		_reloadTextureIndices[reloadQueueIndex] = textureIndex;

		if (targetStreamingLevel > currentStreamingLevel) {
			u32 count = targetStreamingLevel - currentStreamingLevel;
			textureSystem->loadTexture(textureIndex, currentStreamingLevel, count);
		} else {
			textureSystem->loadTexture(textureIndex, 0, targetStreamingLevel);
		}

		_currentStreamingLevels[textureIndex] = targetStreamingLevel;
	}
}

void TextureStreamingSystem::updateStreamingLevel(u32 textureIndex, f32 screenArea) {
	TextureImpl* texture = TextureSystemImpl::Get()->getTexture(textureIndex);
	Application* app = ApplicationSystem::Get()->getApplication();
	u32 screenWidth = app->getScreenWidth();
	u64 sourceWidth = texture->getGpuTexture()->getResourceDesc()._width;
	u32 targetWidth = static_cast<u32>(screenArea * screenWidth);
	u8 requestMipLevel = 0;
	for (u32 i = 1; i < targetWidth; i *= 2) {
		requestMipLevel++;
	}
	requestMipLevel = max(requestMipLevel, 7);

	_targetStreamingLevels[textureIndex] = min(_targetStreamingLevels[textureIndex], requestMipLevel);
}

void TextureStreamingSystem::resetStreamingLevels() {
	memset(_targetStreamingLevels, 0xff, sizeof(u8) * LTN_COUNTOF(_targetStreamingLevels));
}

TextureStreamingSystem* TextureStreamingSystem::Get()
{
	return &_textureStreamingSystem;
}
