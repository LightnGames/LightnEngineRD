#include "GpuTextureManager.h"
#include <Core/Memory.h>
#include <RendererScene/Texture.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <cmath>

namespace ltn {
namespace {
GpuTextureManager g_gpuTextureManager;

void createTextureResource(GpuTexture& gpuTexture, u32 width, u32 height, rhi::Format format) {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	u32 mipLevel = static_cast<u32>(std::log2(max(width, height)) + 1);
	GpuTextureDesc textureDesc = {};
	textureDesc._device = device;
	textureDesc._format = format;
	textureDesc._width = width;
	textureDesc._height = height;
	textureDesc._mipLevels = mipLevel;
	textureDesc._initialState = rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	gpuTexture.initialize(textureDesc);
}
}

void GpuTextureManager::initialize() {
	_textures = Memory::allocObjects<GpuTexture>(TextureScene::TEXTURE_CAPACITY);
	_textureSrv = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->allocate(TextureScene::TEXTURE_CAPACITY);
}

void GpuTextureManager::terminate() {
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_textureSrv);
	Memory::freeObjects(_textures);
}

void GpuTextureManager::update() {
	TextureScene* textureScene = TextureScene::Get();
	const UpdateInfos<Texture>* textureCreateInfos = textureScene->getCreateInfos();
	u32 createCount = textureCreateInfos->getUpdateCount();
	auto createTextures = textureCreateInfos->getObjects();
	for (u32 i = 0; i < createCount; ++i) {
		createTexture(createTextures[i]);
	}

	const UpdateInfos<Texture>* textureDestroyInfos = textureScene->getDestroyInfos();
	u32 destroyCount = textureDestroyInfos->getUpdateCount();
	auto destroyTextures = textureDestroyInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		u32 textureIndex = textureScene->getTextureIndex(destroyTextures[i]);
		_textures[i].terminate();
	}
}

GpuTextureManager* GpuTextureManager::Get() {
	return &g_gpuTextureManager;
}

void GpuTextureManager::createTexture(const Texture* texture) {
	TextureScene* textureScene = TextureScene::Get();
	rhi::Device* device = DeviceManager::Get()->getDevice();
	VramUpdater* vramUpdater = VramUpdater::Get();
	constexpr u32 SUBRESOURCE_COUNT_MAX = 16;
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

	DdsHeaderDxt10 ddsHeaderDxt10;
	assetPath.readFile(&ddsHeaderDxt10, sizeof(DdsHeaderDxt10));

	u32 ddsMipCount = Max(ddsHeader->_mipMapCount, 1U);
	u32 numberOfPlanes = device->getFormatPlaneCount(ddsHeaderDxt10._dxgiFormat);

	bool is3dTexture = ddsHeaderDxt10._resourceDimension == rhi::RESOURCE_DIMENSION_TEXTURE3D;
	bool isCubeMapTexture = false;

	u32 mipOffset = 0;
	u32 mipCount = ddsHeader->_mipMapCount;
	u32 numArray = is3dTexture ? 1 : ddsHeaderDxt10._arraySize;
	u32 targetMipCount = Min(mipOffset + mipCount, ddsMipCount);
	u32 maxResolution = 1 << (targetMipCount - 1);
	u32 clampedWidth = Min(ddsHeader->_width, maxResolution);
	u32 clampedHeight = Min(ddsHeader->_height, maxResolution);
	u32 mipBackLevelOffset = ddsMipCount - targetMipCount;
	u32 mipFrontLevelOffset = mipOffset;

	GpuTexture& gpuTexture = _textures[textureIndex];
	createTextureResource(gpuTexture, clampedWidth, clampedHeight, ddsHeaderDxt10._dxgiFormat);

	// 最大解像度が指定してある場合、それ以外のピクセルを読み飛ばします。
	if (mipBackLevelOffset > 0) {
		rhi::ResourceDesc resourceDesc = gpuTexture.getResourceDesc();
		resourceDesc._width = ddsHeader->_width;
		resourceDesc._height = ddsHeader->_height;
		resourceDesc._mipLevels = ddsMipCount;

		rhi::PlacedSubresourceFootprint layouts[SUBRESOURCE_COUNT_MAX];
		u32 numRows[SUBRESOURCE_COUNT_MAX];
		u64 rowSizesInBytes[SUBRESOURCE_COUNT_MAX];
		u32 numberOfResources = numArray * ddsMipCount * numberOfPlanes;
		device->getCopyableFootprints(&resourceDesc, 0, numberOfResources, 0, layouts, numRows, rowSizesInBytes, nullptr);

		u32 textureFileSeekSizeInbyte = 0;
		for (u32 subResourceIndex = 0; subResourceIndex < mipBackLevelOffset; ++subResourceIndex) {
			const rhi::PlacedSubresourceFootprint& layout = layouts[subResourceIndex];
			u32 rowSizeInBytes = u32(rowSizesInBytes[subResourceIndex]);
			u32 numRow = numRows[subResourceIndex];
			u32 numSlices = layout._footprint._depth;
			textureFileSeekSizeInbyte += sizeof(u8) * rowSizeInBytes * numRow * numSlices;
		}

		LTN_ASSERT(textureFileSeekSizeInbyte > 0);
		assetPath.seekCur(textureFileSeekSizeInbyte);
	}

	u64 requiredSize = 0;

	// サブリソースのレイアウトに沿ってファイルからピクセルデータを読み込み＆ VRAM にアップロード
	{
		rhi::ResourceDesc textureResourceDesc = gpuTexture.getResourceDesc();
		rhi::PlacedSubresourceFootprint layouts[SUBRESOURCE_COUNT_MAX];
		u32 numRows[SUBRESOURCE_COUNT_MAX];
		u64 rowSizesInBytes[SUBRESOURCE_COUNT_MAX];
		u32 numberOfResources = numArray * targetMipCount * numberOfPlanes;
		device->getCopyableFootprints(&textureResourceDesc, 0, numberOfResources, 0, layouts, numRows, rowSizesInBytes, &requiredSize);

		u32 clampedMipCount = Min(mipCount, ddsMipCount);
		void* pixelData = vramUpdater->enqueueUpdate(&gpuTexture, mipOffset, clampedMipCount, nullptr, u32(requiredSize));

		for (u32 subResourceIndex = 0; subResourceIndex < clampedMipCount; ++subResourceIndex) {
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

	// SRV 生成
	device->createShaderResourceView(gpuTexture.getResource(), nullptr, _textureSrv.get(textureIndex)._cpuHandle);
}
}
