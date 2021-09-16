#pragma once
#include <Core/System.h>
#include <Core/Asset.h>
#include <TextureSystem/ModuleSettings.h>

struct TextureDesc {
	const char* _filePath = nullptr;
};

class Texture {
public:
	virtual void requestToDelete() = 0;
	Asset* getAsset() { return _asset; }

protected:
	Asset* _asset = nullptr;
};

class LTN_TEXTURE_SYSTEM_API TextureSystem {
public:
	static constexpr u32 RESIDENT_MIP_COUNT = 7; // 64 px
	static constexpr u32 TEXTURE_COUNT_MAX = 512;
	static constexpr u32 RELOAD_QUEUE_COUNT_MAX = 512;

	virtual Texture* createTexture(const TextureDesc& desc) = 0;
	virtual Texture* allocateTexture(const TextureDesc& desc) = 0;

	static TextureSystem* Get();
};