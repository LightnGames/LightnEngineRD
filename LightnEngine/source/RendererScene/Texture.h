#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"
namespace ltn {
struct DdsPixelFormat {
	u32    _size;
	u32    _flags;
	u32    _fourCC;
	u32    _RGBBitCount;
	u32    _RBitMask;
	u32    _GBitMask;
	u32    _BBitMask;
	u32    _ABitMask;
};

struct DdsHeader {
	u32        _size;
	u32        _flags;
	u32        _height;
	u32        _width;
	u32        _pitchOrLinearSize;
	u32        _depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
	u32        _mipMapCount;
	u32        _reserved1[11];
	DdsPixelFormat _ddspf;
	u32        _caps;
	u32        _caps2;
	u32        _caps3;
	u32        _caps4;
	u32        _reserved2;
};

class Texture {
public:
	static constexpr u32 TEXTURE_HEADER_SIZE_IN_BYTE = sizeof(DdsHeader) + sizeof(u32);

	void setAssetPath(const char* assetPath) { _assetPath = assetPath; }
	DdsHeader* getDdsHeader() { return &_ddsHeader; }

	const char* getAssetPath() const { return _assetPath; }
	const DdsHeader* getDdsHeader() const { return &_ddsHeader; }

private:
	DdsHeader _ddsHeader;
	const char* _assetPath = nullptr;
};

class TextureScene {
public:
	static constexpr u32 TEXTURE_CAPACITY = 1024;

	void initialize();
	void terminate();
	void lateUpdate();

	struct TextureCreatationDesc {
		const char* _assetPath = nullptr;
	};

	const Texture* createTexture(const TextureCreatationDesc& desc);
	void createTextures(const TextureCreatationDesc* descs,Texture const** textures, u32 instanceCount);
	void destroyTexture(const Texture* texture);
	void destroyTextures(Texture const** textures, u32 instanceCount);

	const Texture* findTexture(u64 assetPathHash) const;
	const Texture* getTexture(u32 index) const { return &_textures[index]; }
	u32 getTextureIndex(const Texture* texture) const { return u32(texture - _textures); }

	const UpdateInfos<Texture>* getCreateInfos() const { return &_textureCreateInfos; }
	const UpdateInfos<Texture>* getDestroyInfos() const { return &_textureDestroyInfos; }
	const u8* getTextureEnabledFlags() const { return _textureEnabledFlags; }

	static TextureScene* Get();

private:
	Texture* _textures = nullptr;
	u8* _textureEnabledFlags = nullptr;
	u64* _textureAssetPathHashes = nullptr;
	char** _textureAssetPaths = nullptr;

	VirtualArray _textureAllocations;
	VirtualArray::AllocationInfo* _textureAllocationInfos = nullptr;

	UpdateInfos<Texture> _textureCreateInfos;
	UpdateInfos<Texture> _textureDestroyInfos;
	ChunkAllocator _chunkAllocator;
};
}