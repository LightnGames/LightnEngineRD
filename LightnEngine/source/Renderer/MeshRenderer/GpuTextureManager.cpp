#include "GpuTextureManager.h"
#include <Core/Memory.h>
#include <RendererScene/Texture.h>

namespace ltn {
namespace {
GpuTextureManager g_gpuTextureManager;
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
		struct DdsHeaderDxt10 {
			rhi::Format     _dxgiFormat;
			u32        _resourceDimension;
			u32        _miscFlag; // see D3D11_RESOURCE_MISC_FLAG
			u32        _arraySize;
			u32        _miscFlags2;
		};

		DdsHeaderDxt10 ddsHeaderDxt10;
		fread_s(&ddsHeaderDxt10, sizeof(DdsHeaderDxt10), sizeof(DdsHeaderDxt10), 1, fin);

		u32 ddsMipCount = Max(ddsHeader._mipMapCount, 1U);
		u32 numberOfPlanes = device->getFormatPlaneCount(ddsHeaderDxt10._dxgiFormat);

		bool is3dTexture = ddsHeaderDxt10._resourceDimension == RESOURCE_DIMENSION_TEXTURE3D;
		bool isCubeMapTexture = false;

		u32 numArray = is3dTexture ? 1 : ddsHeaderDxt10._arraySize;
		u32 targetMipCount = min(mipOffset + mipCount, ddsMipCount);
		u32 maxResolution = 1 << (targetMipCount - 1);
		u32 clampedWidth = min(ddsHeader._width, maxResolution);
		u32 clampedHeight = min(ddsHeader._height, maxResolution);
		u32 mipBackLevelOffset = ddsMipCount - targetMipCount;
		u32 mipFrontLevelOffset = mipOffset;

		createTexture(textureIndex, clampedWidth, clampedHeight, ddsHeaderDxt10._dxgiFormat);
		gpuTexture.setDebugName(_debugNames[textureIndex]._name);

		// 最大解像度が指定してある場合、それ以外のピクセルを読み飛ばします。
		if (mipBackLevelOffset > 0) {
			ResourceDesc resourceDesc = gpuTexture.getResourceDesc();
			resourceDesc._width = ddsHeader._width;
			resourceDesc._height = ddsHeader._height;
			resourceDesc._mipLevels = ddsMipCount;

			PlacedSubresourceFootprint layouts[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
			u32 numRows[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
			u64 rowSizesInBytes[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
			u32 numberOfResources = numArray * ddsMipCount * numberOfPlanes;
			device->getCopyableFootprints(&resourceDesc, 0, numberOfResources, 0, layouts, numRows, rowSizesInBytes, nullptr);

			u32 textureFileSeekSizeInbyte = 0;
			for (u32 subResourceIndex = 0; subResourceIndex < mipBackLevelOffset; ++subResourceIndex) {
				const PlacedSubresourceFootprint& layout = layouts[subResourceIndex];
				u32 rowSizeInBytes = static_cast<u32>(rowSizesInBytes[subResourceIndex]);
				u32 numRow = numRows[subResourceIndex];
				u32 numSlices = layout._footprint._depth;
				textureFileSeekSizeInbyte += sizeof(u8) * rowSizeInBytes * numRow * numSlices;
			}

			LTN_ASSERT(textureFileSeekSizeInbyte > 0);
			fseek(fin, textureFileSeekSizeInbyte, SEEK_CUR);
		}

		u64 requiredSize = 0;

		// サブリソースのレイアウトに沿ってファイルからピクセルデータを読み込み＆ VRAM にアップロード
		{
			ResourceDesc textureResourceDesc = gpuTexture.getResourceDesc();
			PlacedSubresourceFootprint layouts[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
			u32 numRows[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
			u64 rowSizesInBytes[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
			u32 numberOfResources = numArray * targetMipCount * numberOfPlanes;
			device->getCopyableFootprints(&textureResourceDesc, 0, numberOfResources, 0, layouts, numRows, rowSizesInBytes, &requiredSize);

			u32 clampedMipCount = min(mipCount, ddsMipCount);
			VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
			void* pixelData = vramUpdater->enqueueUpdate(&gpuTexture, mipOffset, clampedMipCount, nullptr, static_cast<u32>(requiredSize));

			for (u32 subResourceIndex = 0; subResourceIndex < clampedMipCount; ++subResourceIndex) {
				const PlacedSubresourceFootprint& layout = layouts[subResourceIndex];
				u8* data = reinterpret_cast<u8*>(pixelData) + layout._offset;
				u64 rowSizeInBytes = rowSizesInBytes[subResourceIndex];
				u32 numRow = numRows[subResourceIndex];
				u32 rowPitch = layout._footprint._rowPitch;
				u32 slicePitch = layout._footprint._rowPitch * numRow;
				u32 numSlices = layout._footprint._depth;
				for (u32 z = 0; z < numSlices; ++z) {
					u64 slicePitchOffset = static_cast<u64>(slicePitch) * z;
					u8* destSlice = data + slicePitchOffset;
					for (u32 y = 0; y < numRow; ++y) {
						u64 rowPitchOffset = static_cast<u64>(rowPitch) * y;
						fread_s(destSlice + rowPitchOffset, sizeof(u8) * rowSizeInBytes, sizeof(u8), rowSizeInBytes, fin);
					}
				}
			}
		}
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
}
