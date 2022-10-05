#include "GpuTextureManager.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>
#include <RendererScene/Texture.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <cmath>

namespace ltn {
namespace {
GpuTextureManager g_gpuTextureManager;
}

void GpuTextureManager::initialize() {
	CpuScopedPerf scopedPerf("GpuTextureManager");
	_textures = Memory::allocObjects<GpuTexture>(TextureScene::TEXTURE_CAPACITY);
	_textureSrv = DescriptorAllocatorGroup::Get()->allocateSrvCbvUavGpu(TextureScene::TEXTURE_CAPACITY);
	for (u32 i = 0; i < REQUESTED_CREATE_SRV_CAPACITY; ++i) {
		_requestedCreateSrvIndices[i] = REQUESTED_INVALID_INDEX;
	}

	createEmptyTexture(_defaultBlackTexture, 8, 8, 1, rhi::FORMAT_BC1_UNORM_SRGB);

	rhi::Device* device = DeviceManager::Get()->getDevice();
	for (u32 i = 0; i < REQUESTED_CREATE_SRV_CAPACITY; ++i) {
		device->createShaderResourceView(_defaultBlackTexture.getResource(), nullptr, _textureSrv.get(i)._cpuHandle);
	}
}

void GpuTextureManager::terminate() {
	update();
	DescriptorAllocatorGroup::Get()->deallocSrvCbvUavGpu(_textureSrv);
	Memory::deallocObjects(_textures);
	_defaultBlackTexture.terminate();
}

void GpuTextureManager::update() {
	TextureScene* textureScene = TextureScene::Get();
	const UpdateInfos<Texture>* textureCreateInfos = textureScene->getCreateInfos();
	u32 createCount = textureCreateInfos->getUpdateCount();
	auto createTextures = textureCreateInfos->getObjects();
	for (u32 i = 0; i < createCount; ++i) {
		const Texture* texture = createTextures[i];

		// テクスチャストリーミングが無効なテクスチャをロード
		if (!texture->isStreamingDisabled()) {
			continue;
		}
		const DdsHeader* ddsHeader = createTextures[i]->getDdsHeader();
		createTexture(createTextures[i], 0, ddsHeader->_mipMapCount);
		createTextureSrv(createTextures[i]);
	}

	const UpdateInfos<Texture>* textureDestroyInfos = textureScene->getDestroyInfos();
	u32 destroyCount = textureDestroyInfos->getUpdateCount();
	auto destroyTextures = textureDestroyInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		u32 textureIndex = textureScene->getTextureIndex(destroyTextures[i]);
		LTN_ASSERT(textureIndex < TextureScene::TEXTURE_CAPACITY);
		if (_textures[textureIndex].isVaild()) {
			ReleaseQueue::Get()->enqueue(_textures[textureIndex]);
			_textures[textureIndex] = GpuTexture();
		}
	}

	// SRV 遅延生成
	for (u32 i = 0; i < REQUESTED_CREATE_SRV_CAPACITY; ++i) {
		if (_requestedCreateSrvTimers[i] == 0) {
			continue;
		}

		if (--_requestedCreateSrvTimers[i] == 0) {
			// SRV 再生成
			const Texture* texture = TextureScene::Get()->getTexture(_requestedCreateSrvIndices[i]);
			createTextureSrv(texture);
			_requestedCreateSrvIndices[i] = REQUESTED_INVALID_INDEX;
		}
	}
}

void GpuTextureManager::createEmptyTexture(GpuTexture& gpuTexture, u32 width, u32 height, u32 depth, rhi::Format format) {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	u32 mipLevel = u32(std::log2(Max(width, height)) + 1);
	GpuTextureDesc textureDesc = {};
	textureDesc._device = device;
	textureDesc._format = format;
	textureDesc._width = width;
	textureDesc._height = height;
	textureDesc._depthOrArraySize = depth;
	textureDesc._mipLevels = mipLevel;
	textureDesc._initialState = rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	textureDesc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	gpuTexture.initialize(textureDesc);
}

void GpuTextureManager::streamTexture(const Texture* texture, u32 currentStreamMipLevel, u32 targetStreamMipLevel) {
	// 古いテクスチャのコピーを取って破棄
	TextureScene* textureScene = TextureScene::Get();
	u32 textureIndex = textureScene->getTextureIndex(texture);
	GpuTexture oldGpuTexture = _textures[textureIndex];
	if (currentStreamMipLevel > 0) {
		LTN_ASSERT(oldGpuTexture.isVaild());
		ReleaseQueue::Get()->enqueue(oldGpuTexture);
	}

	if (targetStreamMipLevel > currentStreamMipLevel) {
		u8 loadMipCount = targetStreamMipLevel - currentStreamMipLevel;

		// 追加読み込みする範囲の IO 読み取りだけしてテクスチャ作成
		createTexture(texture, currentStreamMipLevel, loadMipCount);

		// すでに読み込まれているテクスチャを古いテクスチャからコピー
		if (currentStreamMipLevel > 0) {
			u8 dstSubResourceOffset = loadMipCount;
			copyTexture(&_textures[textureIndex], &oldGpuTexture, 0, currentStreamMipLevel , 0, dstSubResourceOffset);
		}
	} else {
		u32 width = 1 << (targetStreamMipLevel - 1);

		// 縮小後の新しいサイズの空テクスチャ作成
		GpuTexture& gpuTexture = _textures[textureIndex];
		createEmptyTexture(gpuTexture, width, width, 1, oldGpuTexture.getResourceDesc()._format);
		gpuTexture.setName(texture->getAssetPath());

		// 古いテクスチャから縮小後の範囲だけテクスチャコピー
		if (currentStreamMipLevel > 0) {
			u8 srcSubResourceOffset = currentStreamMipLevel - targetStreamMipLevel;
			copyTexture(&_textures[textureIndex], &oldGpuTexture, 0, targetStreamMipLevel, srcSubResourceOffset, 0);
		}
	}
}

void GpuTextureManager::requestCreateSrv(u32 textureIndex) {
	u32 findIndex = REQUESTED_INVALID_INDEX;
	for (u32 i = 0; i < REQUESTED_CREATE_SRV_CAPACITY; ++i) {
		if (_requestedCreateSrvIndices[i] == REQUESTED_INVALID_INDEX) {
			findIndex = i;
			break;
		}
	}

	LTN_ASSERT(findIndex != REQUESTED_INVALID_INDEX);
	u32 requestedIndex = findIndex;
	_requestedCreateSrvIndices[requestedIndex] = textureIndex;
	_requestedCreateSrvTimers[requestedIndex] = rhi::BACK_BUFFER_COUNT - 1;
}

GpuTextureManager* GpuTextureManager::Get() {
	return &g_gpuTextureManager;
}

void GpuTextureManager::createTextureSrv(const Texture* texture) {
	u32 textureIndex = TextureScene::Get()->getTextureIndex(texture);
	GpuTexture& gpuTexture = _textures[textureIndex];
	rhi::Device* device = DeviceManager::Get()->getDevice();
	rhi::ShaderResourceViewDesc overrideSrvDesc = {};
	rhi::ShaderResourceViewDesc* srvDesc = nullptr;
	if (texture->getTextureType() == Texture::CUBE_MAP) {
		overrideSrvDesc._viewDimension = rhi::SRV_DIMENSION_TEXTURECUBE;
		overrideSrvDesc._format = gpuTexture.getResourceDesc()._format;
		overrideSrvDesc._textureCube._mipLevels = gpuTexture.getResourceDesc()._mipLevels;
		srvDesc = &overrideSrvDesc;
	}

	device->createShaderResourceView(gpuTexture.getResource(), srvDesc, _textureSrv.get(textureIndex)._cpuHandle);
}

void GpuTextureManager::createTexture(const Texture* texture, u32 beginMipOffset, u32 loadMipCount) {
	TextureScene* textureScene = TextureScene::Get();
	rhi::Device* device = DeviceManager::Get()->getDevice();
	VramUpdater* vramUpdater = VramUpdater::Get();
	constexpr u32 SUBRESOURCE_COUNT_MAX = 96;
	u32 textureIndex = textureScene->getTextureIndex(texture);

	const DdsHeader* ddsHeader = texture->getDdsHeader();
	struct DdsHeaderDxt10 {
		rhi::Format     _dxgiFormat;
		u32        _resourceDimension;
		u32        _miscFlag; // see D3D11_RESOURCE_MISC_FLAG
		u32        _arraySize;
		u32        _miscFlags2;
	};

	AssetPath assetPath(texture->getAssetPath());
	assetPath.openFile();
	assetPath.seekSet(Texture::TEXTURE_HEADER_SIZE_IN_BYTE);

	u32 ddsMipCount = Max(ddsHeader->_mipMapCount, 1U);
	u32 arraySize = 1;
	rhi::Format format = rhi::FORMAT_UNKNOWN;
	bool isCubeMapTexture = false;
	bool is3dTexture = false;
	{
		DdsHeaderDxt10 ddsHeaderDxt10;
		assetPath.readFile(&ddsHeaderDxt10, sizeof(DdsHeaderDxt10));

		is3dTexture = ddsHeaderDxt10._resourceDimension == rhi::RESOURCE_DIMENSION_TEXTURE3D;
		isCubeMapTexture = ddsHeaderDxt10._miscFlag & 0x4 /* RESOURCE_MISC_TEXTURECUBE */;
		arraySize = ddsHeaderDxt10._arraySize;
		format = ddsHeaderDxt10._dxgiFormat;
	}

	u32 numberOfPlanes = device->getFormatPlaneCount(format);
	u32 mipOffset = beginMipOffset;
	u32 mipCount = loadMipCount;
	u32 numArray = is3dTexture ? 1 : arraySize;
	u32 targetMipCount = Min(mipOffset + mipCount, ddsMipCount);
	u32 maxResolution = 1 << (targetMipCount - 1);
	u32 clampedWidth = Min(ddsHeader->_width, maxResolution);
	u32 clampedHeight = Min(ddsHeader->_height, maxResolution);
	u32 mipBackLevelOffset = ddsMipCount - targetMipCount;
	u32 mipFrontLevelOffset = mipOffset;

	if (isCubeMapTexture) {
		numArray *= 6;
	}

	GpuTexture& gpuTexture = _textures[textureIndex];
	createEmptyTexture(gpuTexture, clampedWidth, clampedHeight, numArray, format);
	gpuTexture.setName(texture->getAssetPath());

	// 最大解像度が指定してある場合、それ以外のピクセルを読み飛ばします。
	if (mipBackLevelOffset > 0) {
		rhi::ResourceDesc resourceDesc = gpuTexture.getResourceDesc();
		resourceDesc._width = ddsHeader->_width;
		resourceDesc._height = ddsHeader->_height;
		resourceDesc._mipLevels = ddsMipCount;

		rhi::PlacedSubresourceFootprint layouts[SUBRESOURCE_COUNT_MAX];
		u32 numRows[SUBRESOURCE_COUNT_MAX];
		u64 rowSizesInBytes[SUBRESOURCE_COUNT_MAX];
		u32 subResourceCount = numArray * ddsMipCount * numberOfPlanes;
		LTN_ASSERT(subResourceCount < SUBRESOURCE_COUNT_MAX);

		device->getCopyableFootprints(&resourceDesc, 0, subResourceCount, 0, layouts, numRows, rowSizesInBytes, nullptr);

		u32 textureFileSeekSizeInbyte = 0;
		for (u32 subResourceIndex = 0; subResourceIndex < mipBackLevelOffset; ++subResourceIndex) {
			const rhi::PlacedSubresourceFootprint& layout = layouts[subResourceIndex];
			u64 rowSizeInBytes = rowSizesInBytes[subResourceIndex];
			u32 numRow = numRows[subResourceIndex];
			u32 numSlices = layout._footprint._depth;
			textureFileSeekSizeInbyte += u32(rowSizeInBytes) * numSlices * numRow;
		}

		LTN_ASSERT(textureFileSeekSizeInbyte > 0);
		assetPath.seekCur(textureFileSeekSizeInbyte);
	}


	// サブリソースのレイアウトに沿ってファイルからピクセルデータを読み込み＆ VRAM にアップロード
	{
		rhi::ResourceDesc textureResourceDesc = gpuTexture.getResourceDesc();
		rhi::PlacedSubresourceFootprint layouts[SUBRESOURCE_COUNT_MAX];
		u32 numRows[SUBRESOURCE_COUNT_MAX];
		u64 rowSizesInBytes[SUBRESOURCE_COUNT_MAX];
		u32 subResourceCount = numArray * targetMipCount * numberOfPlanes;
		LTN_ASSERT(subResourceCount < SUBRESOURCE_COUNT_MAX);

		u64 requiredSize = 0;
		device->getCopyableFootprints(&textureResourceDesc, 0, subResourceCount, 0, layouts, numRows, rowSizesInBytes, &requiredSize);

		u32 uploadSubResourceCount = Min(mipCount, ddsMipCount) * numArray;
		void* pixelData = vramUpdater->enqueueUpdate(&gpuTexture, mipOffset, uploadSubResourceCount, nullptr, u32(requiredSize));

		for (u32 subResourceIndex = 0; subResourceIndex < uploadSubResourceCount; ++subResourceIndex) {
			const rhi::PlacedSubresourceFootprint& layout = layouts[subResourceIndex];
			u8* data = reinterpret_cast<u8*>(pixelData) + layout._offset;
			u64 rowSizeInBytes = rowSizesInBytes[subResourceIndex];
			u32 numRow = numRows[subResourceIndex];
			u32 rowPitch = layout._footprint._rowPitch;
			u32 slicePitch = layout._footprint._rowPitch * numRow;
			u32 numSlices = layout._footprint._depth;
			for (u32 z = 0; z < numSlices; ++z) {
				u64 slicePitchOffset = u64(slicePitch) * z;
				u8* destSlice = data + slicePitchOffset;
				for (u32 y = 0; y < numRow; ++y) {
					u64 rowPitchOffset = u64(rowPitch) * y;
					assetPath.readFile(destSlice + rowPitchOffset, u32(rowSizeInBytes));
				}
			}
		}
	}

	assetPath.closeFile();
}

void GpuTextureManager::copyTexture(GpuTexture* dstTexture, GpuTexture* srcTexture, u32 firstSubResource, u32 subResourceCount, s32 srcSubResourceOffset, s32 dstSubResourceOffset) {
	VramUpdater* vramUpdater = VramUpdater::Get();
	for (u32 i = 0; i < subResourceCount; ++i) {
		u32 srcSubResourceIndex = firstSubResource + i;
		vramUpdater->enqueueUpdate(dstTexture, srcTexture, srcSubResourceIndex + srcSubResourceOffset, srcSubResourceIndex + dstSubResourceOffset);
	}
}
}
