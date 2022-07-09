#include "VramUpdater.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/RendererUtility.h>

namespace ltn {
namespace {
VramUpdater g_vramUpdater;
}
void VramUpdater::initialize() {
	GpuDynamicBufferDesc desc = {};
	desc._device = DeviceManager::Get()->getDevice();
	desc._sizeInByte = STAGING_BUFFER_SIZE_IN_BYTE;
	_stagingBuffer.initialize(desc);
	_stagingBuffer.setName("VramUpdaterStaging");
	_stagingMapPtr = _stagingBuffer.map<u8>();

	// 範囲外アクセス検知用マーカーを最初に挿入
	*reinterpret_cast<u32*>(_stagingMapPtr) = OVER_RUN_MARKER;
}

void VramUpdater::terminate() {
	_stagingBuffer.terminate();
}

void VramUpdater::update(u32 frameIndex) {
	_frameStarts[frameIndex] = _currentBufferStart;

	u32 prevFrameIndex = (frameIndex + rhi::BACK_BUFFER_COUNT - 1) % rhi::BACK_BUFFER_COUNT;
	_prevBufferStart = _frameStarts[prevFrameIndex];
}

void VramUpdater::enqueueUpdate(GpuResource* dstBuffer, u32 dstOffset, GpuResource* sourceBuffer, u32 sourceOffset, u32 copySizeInByte) {
	LTN_ASSERT(copySizeInByte > 0);
	LTN_ASSERT(dstBuffer->getSizeInByte() > sourceOffset + copySizeInByte);

	u32 headerIndex = _updateHeaderCount++; // atomic incriment
	UpdateHeader& header = _updateHeaders[headerIndex];
	header._dstResource = dstBuffer;
	header._dstOffset = dstOffset;
	header._sourceResource = sourceBuffer;
	header._sourceOffset = sourceOffset;
	header._copySizeInByte = copySizeInByte;
	LTN_ASSERT(headerIndex < BUFFER_HEADER_COUNT_MAX);
}

void VramUpdater::enqueueUpdate(GpuTexture* dstTexture, GpuTexture* srcTexture, u32 srcSubResourceIndex, u32 dstSubResourceIndex) {
	u32 headerIndex = _textureCopyHeaderCount++; // atomic incriment
	TextureCopyHeader& header = _textureCopyHeaders[headerIndex];
	header._dstTexture = dstTexture;
	header._srcTexture = *srcTexture;
	header._srcSubResourceIndex = srcSubResourceIndex;
	header._dstSubResourceIndex = dstSubResourceIndex;
	header._srcTextureUniqueMarker = srcTexture;
}

void* VramUpdater::enqueueUpdate(GpuResource* dstBuffer, u32 dstOffset, u32 copySizeInByte) {
	LTN_ASSERT(copySizeInByte > 0);
	u32 headerIndex = _stagingUpdateHeaderCount++; // atomic incriment
	void* stagingBufferPtr = allocateUpdateBuffer(copySizeInByte, 4);

	StagingUpdateHeader& header = _stagingUpdateHeaders[headerIndex];
	header._dstResource = dstBuffer;
	header._dstOffset = dstOffset;
	header._copySizeInByte = copySizeInByte;
	header._stagingBufferOffset = getStagingBufferStartOffset(stagingBufferPtr);
	LTN_ASSERT(header._stagingBufferOffset + copySizeInByte <= _stagingBuffer.getSizeInByte());
	LTN_ASSERT(headerIndex < BUFFER_HEADER_COUNT_MAX);

	return stagingBufferPtr;
}

void* VramUpdater::enqueueUpdate(GpuTexture* dstTexture, u32 firstSubResourceIndex, u32 subResourceCount, const rhi::SubResourceData* subResources, u32 copySizeInByte) {
	LTN_ASSERT(subResourceCount > 0);
	LTN_ASSERT(copySizeInByte > 0);
	u32 headerIndex = _textureUpdateHeaderCount++; // atomic incriment
	void* stagingBufferPtr = allocateUpdateBuffer(copySizeInByte, rhi::GetTextureBufferAligned(copySizeInByte));

	TextureUpdateHeader& header = _textureUpdateHeaders[headerIndex];
	header._dstTexture = dstTexture;
	header._numSubresources = subResourceCount;
	header._firstSubResources = firstSubResourceIndex;
	header._stagingBufferOffset = getStagingBufferStartOffset(stagingBufferPtr);
	LTN_ASSERT(header._stagingBufferOffset + copySizeInByte <= _stagingBuffer.getSizeInByte());
	LTN_ASSERT(headerIndex < BUFFER_HEADER_COUNT_MAX);

	return stagingBufferPtr;
}

void VramUpdater::populateCommandList(rhi::CommandList* commandList) {
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "VramUpdater");
	constexpr u32 BARRIER_COUNT_MAX = 128;
	constexpr u32 UNKNOWN_RESOURCE_INDEX = 0xffffffff;
	void* uniqueResources[BARRIER_COUNT_MAX] = {};
	rhi::ResourceTransitionBarrier barriers[BARRIER_COUNT_MAX] = {};
	u32 barrierCount = 0;

	// 重複リソース検索ラムダ
	auto findUniqueResource = [&uniqueResources, &barrierCount, UNKNOWN_RESOURCE_INDEX](void* resource) {
		for (u32 resourceIndex = 0; resourceIndex < barrierCount; ++resourceIndex) {
			if (uniqueResources[resourceIndex] == resource) {
				return resourceIndex;
			}
		}

		return UNKNOWN_RESOURCE_INDEX;
	};

	// vram to vram
	for (u32 headerIndex = 0; headerIndex < _updateHeaderCount; ++headerIndex) {
		UpdateHeader& header = _updateHeaders[headerIndex];
		u32 dstResourceIndex = findUniqueResource(header._dstResource);
		if (dstResourceIndex == UNKNOWN_RESOURCE_INDEX) {
			barriers[barrierCount] = header._dstResource->getTransitionBarrier(rhi::RESOURCE_STATE_COPY_DEST);
			uniqueResources[barrierCount++] = header._dstResource;
		}

		u32 sourceResourceIndex = findUniqueResource(header._sourceResource);
		if (sourceResourceIndex == UNKNOWN_RESOURCE_INDEX) {
			barriers[barrierCount] = header._sourceResource->getTransitionBarrier(rhi::RESOURCE_STATE_COPY_SOURCE);
			uniqueResources[barrierCount++] = header._sourceResource;
		}
	}

	// staging to vram
	for (u32 headerIndex = 0; headerIndex < _stagingUpdateHeaderCount; ++headerIndex) {
		StagingUpdateHeader& header = _stagingUpdateHeaders[headerIndex];
		u32 dstResourceIndex = findUniqueResource(header._dstResource);
		if (dstResourceIndex == UNKNOWN_RESOURCE_INDEX) {
			barriers[barrierCount] = header._dstResource->getTransitionBarrier(rhi::RESOURCE_STATE_COPY_DEST);
			uniqueResources[barrierCount++] = header._dstResource;
		}
	}

	// texture to vram
	for (u32 headerIndex = 0; headerIndex < _textureUpdateHeaderCount; ++headerIndex) {
		TextureUpdateHeader& header = _textureUpdateHeaders[headerIndex];
		u32 dstResourceIndex = findUniqueResource(header._dstTexture);
		if (dstResourceIndex == UNKNOWN_RESOURCE_INDEX) {
			barriers[barrierCount] = header._dstTexture->getTransitionBarrier(rhi::RESOURCE_STATE_COPY_DEST);
			uniqueResources[barrierCount++] = header._dstTexture;
		}
	}

	// texture to texture
	for (u32 headerIndex = 0; headerIndex < _textureCopyHeaderCount; ++headerIndex) {
		TextureCopyHeader& header = _textureCopyHeaders[headerIndex];
		// Src テクスチャはコピーを取ってあるのでユニークリソース識別子を利用
		void* srcTextureUniqueMarker = reinterpret_cast<void*>(header._srcTexture.getResource()->getUniqueMarker());
		u32 srcResourceIndex = findUniqueResource(srcTextureUniqueMarker);
		if (srcResourceIndex == UNKNOWN_RESOURCE_INDEX) {
			barriers[barrierCount] = header._srcTexture.getTransitionBarrier(rhi::RESOURCE_STATE_COPY_SOURCE);
			uniqueResources[barrierCount++] = srcTextureUniqueMarker;
		}

		u32 dstResourceIndex = findUniqueResource(header._dstTexture);
		if (dstResourceIndex == UNKNOWN_RESOURCE_INDEX) {
			barriers[barrierCount] = header._dstTexture->getTransitionBarrier(rhi::RESOURCE_STATE_COPY_DEST);
			uniqueResources[barrierCount++] = header._dstTexture;
		}
	}

	// コピー可能リソースステートに変更
	if (barrierCount > 0) {
		commandList->transitionBarriers(barriers, barrierCount);
	}

	// Vram to vram
	for (u32 headerIndex = 0; headerIndex < _updateHeaderCount; ++headerIndex) {
		UpdateHeader& header = _updateHeaders[headerIndex];
		commandList->copyBufferRegion(header._dstResource->getResource(), header._dstOffset, header._sourceResource->getResource(), header._sourceOffset, header._copySizeInByte);
	}

	// Staging to vram
	for (u32 headerIndex = 0; headerIndex < _stagingUpdateHeaderCount; ++headerIndex) {
		StagingUpdateHeader& header = _stagingUpdateHeaders[headerIndex];
		commandList->copyBufferRegion(header._dstResource->getResource(), header._dstOffset, _stagingBuffer.getResource(), header._stagingBufferOffset, header._copySizeInByte);
	}

	// Texture to vram
	for (u32 headerIndex = 0; headerIndex < _textureUpdateHeaderCount; ++headerIndex) {
		TextureUpdateHeader& header = _textureUpdateHeaders[headerIndex];
		rhi::Device* device = DeviceManager::Get()->getDevice();
		rhi::ResourceDesc textureDesc = header._dstTexture->getResourceDesc();
		u64 requiredSize = 0;
		u32 subResourceCount = header._firstSubResources + header._numSubresources;
		rhi::PlacedSubresourceFootprint layouts[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX];
		u32 numRows[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX];
		u64 rowSizesInBytes[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX];
		device->getCopyableFootprints(&textureDesc, 0, subResourceCount, header._stagingBufferOffset, layouts, numRows, rowSizesInBytes, &requiredSize);

		for (u32 subResourceIndex = 0; subResourceIndex < header._numSubresources; ++subResourceIndex) {
			rhi::TextureCopyLocation src = {};
			src._resource = _stagingBuffer.getResource();
			src._placedFootprint = layouts[subResourceIndex];
			src._type = rhi::TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

			rhi::TextureCopyLocation dst = {};
			dst._resource = header._dstTexture->getResource();
			dst._subresourceIndex = subResourceIndex;
			dst._type = rhi::TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

			commandList->copyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}
	}

	// Texture to texture
	for (u32 headerIndex = 0; headerIndex < _textureCopyHeaderCount; ++headerIndex) {
		TextureCopyHeader& header = _textureCopyHeaders[headerIndex];
		rhi::TextureCopyLocation src = {};
		src._resource = header._srcTexture.getResource();
		src._subresourceIndex = header._srcSubResourceIndex;
		src._type = rhi::TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		rhi::TextureCopyLocation dst = {};
		dst._resource = header._dstTexture->getResource();
		dst._subresourceIndex = header._dstSubResourceIndex;
		dst._type = rhi::TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		commandList->copyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	// 変更したリソースを元に戻すバリアを構築
	for (u32 barrierIndex = 0; barrierIndex < barrierCount; ++barrierIndex) {
		rhi::ResourceTransitionBarrier& barrier = barriers[barrierIndex];
		rhi::ResourceStates beforeState = barrier._stateBefore;
		barrier._stateBefore = barrier._stateAfter;
		barrier._stateAfter = beforeState;
	}

	// コピー可能ステートから元のステートに戻す
	if (barrierCount > 0) {
		commandList->transitionBarriers(barriers, barrierCount);
	}

	_uploadSizeInByte = 0;
	_updateHeaderCount = 0;
	_stagingUpdateHeaderCount = 0;
	_textureUpdateHeaderCount = 0;
	_textureCopyHeaderCount = 0;
}

VramUpdater* VramUpdater::Get() {
	return &g_vramUpdater;
}

void* VramUpdater::allocateUpdateBuffer(u32 sizeInByte, u32 alignment) {
	u8* allocatedPtr = nullptr;
	u32 alignedCurrentBufferStart = GetAligned(_currentBufferStart, alignment);
	if (_currentBufferStart % alignment == 0) {
		alignedCurrentBufferStart = _currentBufferStart;
	}

	if (alignedCurrentBufferStart < _prevBufferStart) {
		LTN_ASSERT(alignedCurrentBufferStart + sizeInByte < _prevBufferStart);
	}

	if (alignedCurrentBufferStart + sizeInByte <= STAGING_BUFFER_SIZE_IN_BYTE) {
		allocatedPtr = _stagingMapPtr + alignedCurrentBufferStart;
		_currentBufferStart = alignedCurrentBufferStart + sizeInByte;
		_uploadSizeInByte += sizeInByte;
		return allocatedPtr;
	}

	// バッファの端まで使っていたら先頭に戻る
	if (sizeInByte < _prevBufferStart) {
		allocatedPtr = _stagingMapPtr;
		_currentBufferStart = sizeInByte;
		_uploadSizeInByte += sizeInByte;
		return allocatedPtr;
	}

	// メモリが確保できない
	return nullptr;
}

u32 VramUpdater::getStagingBufferStartOffset(const void* allocatedPtr) const {
	LTN_ASSERT(allocatedPtr != nullptr);
	const u8* ptr = reinterpret_cast<const u8*>(allocatedPtr);
	return static_cast<u32>(ptr - _stagingMapPtr);
}
}
