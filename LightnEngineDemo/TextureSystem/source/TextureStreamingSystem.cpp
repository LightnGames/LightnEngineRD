#include <TextureSystem/impl/TextureStreamingSystem.h>
#include <TextureSystem/impl/TextureSystemImpl.h>
#include <Core/Application.h>

TextureStreamingSystem _textureStreamingSystem;
void TextureStreamingSystem::initialize()
{
}

void TextureStreamingSystem::terminate()
{
}

void TextureStreamingSystem::update()
{
	TextureSystemImpl* textureSystem = TextureSystemImpl::Get();
	u32 textureCount = textureSystem->getTextureResarveCount();
	for (u32 textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
		u32 targetStreamingLevel = _targetStreamingLevels[textureIndex];
		if (_currentStreamingLevels[textureIndex] == targetStreamingLevel) {
			continue;
		}

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
	_targetStreamingLevels[textureIndex] = requestMipLevel;
}

TextureStreamingSystem* TextureStreamingSystem::Get()
{
	return &_textureStreamingSystem;
}
