#include "Texture.h"
#include <Core/Memory.h>

namespace ltn {
namespace {
TextureScene g_textureScene;
}

void TextureScene::initialize() {
	VirtualArray::Desc handleDesc = {};
	handleDesc._size = TEXTURE_CAPACITY;
	_textureAllocations.initialize(handleDesc);

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_textures = allocation.allocateObjects<Texture>(TEXTURE_CAPACITY);
		_textureAssetPathHashes = allocation.allocateObjects<u64>(TEXTURE_CAPACITY);
		_textureAssetPaths = allocation.allocateObjects<char*>(TEXTURE_CAPACITY);
		_textureAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(TEXTURE_CAPACITY);
		});
}

void TextureScene::terminate() {
	_textureAllocations.terminate();
	_chunkAllocator.free();
}

void TextureScene::lateUpdate() {
	u32 destroyShaderCount = _textureDestroyInfos.getUpdateCount();
	auto destroyTextures = _textureDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyShaderCount; ++i) {
		u32 shaderIndex = getTextureIndex(destroyTextures[i]);
		_textureAllocations.freeAllocation(_textureAllocationInfos[shaderIndex]);
		Memory::freeObjects(_textureAssetPaths[i]);

		_textures[shaderIndex] = Texture();
	}

	_textureCreateInfos.reset();
	_textureDestroyInfos.reset();
}

const Texture* TextureScene::createTexture(const TextureCreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _textureAllocations.allocation(1);

	_textureAllocationInfos[allocationInfo._offset] = allocationInfo;
	u32 assetPathLength = StrLength(desc._assetPath) + 1;

	char*& shaderAssetPath = _textureAssetPaths[allocationInfo._offset];
	shaderAssetPath = Memory::allocObjects<char>(assetPathLength);
	memcpy(shaderAssetPath, desc._assetPath, assetPathLength);

	Texture* texture = &_textures[allocationInfo._offset];
	texture->setAssetPath(_textureAssetPaths[allocationInfo._offset]);

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

	_textureAssetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);
	_textureCreateInfos.push(texture);
	return texture;
}

void TextureScene::destroyTexture(const Texture* texture) {
	_textureDestroyInfos.push(texture);
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
