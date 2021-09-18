#include <TextureSystem/impl/TextureStreamingSystem.h>
#include <TextureSystem/impl/TextureSystemImpl.h>
#include <Core/Application.h>

TextureStreamingSystem _textureStreamingSystem;
void TextureStreamingSystem::initialize() {}

void TextureStreamingSystem::terminate() {}

void TextureStreamingSystem::update() {
	TextureSystemImpl* textureSystem = TextureSystemImpl::Get();

	// �Đ����v���Őݒ肵��������������V�����e�N�X�`��SRV�ɍ����ւ��ČÂ����\�[�X��j��
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

	// �v���X�g���[�~���O���x�������݂̃��x���ƈႤ�e�N�X�`���ɑ΂��čĐ����v��
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

		// �R�������\�[�X�̓X�g���[�~���O���Ȃ�
		// TODO: �e�N�X�`�����ƂɃX�g���[�~���O�ݒ��ǉ�����
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

		// ���x�����オ�����獷�������[�h���Ă���łɓǂݍ��܂�Ă���͈͂��R�s�[
		if (targetStreamingLevel > currentStreamingLevel) {
			s32 dstSubResourceOffset = targetStreamingLevel - currentStreamingLevel;
			textureSystem->loadTexture(textureIndex, currentStreamingLevel, dstSubResourceOffset);
			textureSystem->copyTexture(textureIndex, &currentTexture, 0, currentStreamingLevel, 0, dstSubResourceOffset);
		} else {
			// ���x��������������k�����ꂽ�e�N�X�`���ɃR�s�[
			u32 size = 1 << (targetStreamingLevel - 1);
			u32 subResourceOffset = currentStreamingLevel - targetStreamingLevel;
			textureSystem->createTexture(textureIndex, size, size, currentTexture.getResourceDesc()._format);
			textureSystem->copyTexture(textureIndex, &currentTexture, 0, targetStreamingLevel, subResourceOffset, 0);
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
	requestMipLevel = max(requestMipLevel, TextureSystem::RESIDENT_MIP_COUNT);

	_targetStreamingLevels[textureIndex] = min(_targetStreamingLevels[textureIndex], requestMipLevel);
}

void TextureStreamingSystem::resetStreamingLevels() {
	memset(_targetStreamingLevels, 0xff, sizeof(u8) * LTN_COUNTOF(_targetStreamingLevels));
}

TextureStreamingSystem* TextureStreamingSystem::Get()
{
	return &_textureStreamingSystem;
}
