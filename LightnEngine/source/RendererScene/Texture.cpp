#include "Texture.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>

namespace ltn {
namespace {
TextureScene g_textureScene;
}

void TextureScene::initialize() {
	CpuScopedPerf scopedPerf("TextureScene");
	VirtualArray::Desc handleDesc = {};
	handleDesc._size = TEXTURE_CAPACITY;
	_textureAllocations.initialize(handleDesc);

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_textures = allocation.allocateObjects<Texture>(TEXTURE_CAPACITY);
		_textureAssetPathHashes = allocation.allocateObjects<u64>(TEXTURE_CAPACITY);
		_textureAssetPaths = allocation.allocateObjects<char*>(TEXTURE_CAPACITY);
		_textureAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(TEXTURE_CAPACITY);
		_textureEnabledFlags = allocation.allocateClearedObjects<u8>(TEXTURE_CAPACITY);
	});
}

void TextureScene::terminate() {
	_textureAllocations.terminate();
	_chunkAllocator.free();
}

void TextureScene::lateUpdate() {
	u32 destroyCount = _textureDestroyInfos.getUpdateCount();
	auto destroyTextures = _textureDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		u32 textureIndex = getTextureIndex(destroyTextures[i]);
		LTN_ASSERT(textureIndex < TEXTURE_CAPACITY);
		LTN_ASSERT(_textureEnabledFlags[textureIndex] == 1);
		_textureAllocations.freeAllocation(_textureAllocationInfos[textureIndex]);
		_textureEnabledFlags[textureIndex] = 0;
		Memory::deallocObjects(_textureAssetPaths[i]);

		_textures[textureIndex] = Texture();
	}

	_textureCreateInfos.reset();
	_textureDestroyInfos.reset();
}

const Texture* TextureScene::createTexture(const TextureCreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _textureAllocations.allocation(1);

	_textureAllocationInfos[allocationInfo._offset] = allocationInfo;
	u32 assetPathLength = StrLength(desc._assetPath) + 1;

	char*& assetPath = _textureAssetPaths[allocationInfo._offset];
	assetPath = Memory::allocObjects<char>(assetPathLength);
	memcpy(assetPath, desc._assetPath, assetPathLength);

	Texture* texture = &_textures[allocationInfo._offset];
	texture->setAssetPath(_textureAssetPaths[allocationInfo._offset]);
	texture->setStreamingDisabled(desc._streamingDisabled);
	texture->setTextureType(desc._textureType);

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                          \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#define DDS_FOURCC      0x00000004  // DDPF_FOURCC
	const u32 DDS_MAGIC = 0x20534444; // "DDS "

	// テクスチャヘッダーだけ読み取り
	{
		AssetPath assetPath(desc._assetPath);
		assetPath.openFile();

		u32 fileMagic = 0;
		assetPath.readFile(&fileMagic, sizeof(u32));
		LTN_ASSERT(fileMagic == DDS_MAGIC);

		DdsHeader* ddsHeader = texture->getDdsHeader();
		assetPath.readFile(ddsHeader, sizeof(DdsHeader));
		LTN_ASSERT(ddsHeader->_ddspf._flags & DDS_FOURCC);
		LTN_ASSERT(MAKEFOURCC('D', 'X', '1', '0') == ddsHeader->_ddspf._fourCC);
		assetPath.closeFile();
	}

	_textureEnabledFlags[allocationInfo._offset] = 1;
	_textureAssetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);
	_textureCreateInfos.push(texture);
	return texture;
}

void TextureScene::createTextures(const TextureCreatationDesc* descs, Texture const** textures, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		const TextureCreatationDesc* desc = descs + i;
		textures[i] = createTexture(*desc);
	}
}

void TextureScene::destroyTexture(const Texture* texture) {
	_textureDestroyInfos.push(texture);
}

void TextureScene::destroyTextures(Texture const** textures, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		destroyTexture(textures[i]);
	}
}

const Texture* TextureScene::findTexture(u64 assetPathHash) const {
	for (u32 i = 0; i < TEXTURE_CAPACITY; ++i) {
		if (_textureAssetPathHashes[i] == assetPathHash) {
			return &_textures[i];
		}
	}

	return nullptr;
}

TextureScene* TextureScene::Get() {
	return &g_textureScene;
}
}
