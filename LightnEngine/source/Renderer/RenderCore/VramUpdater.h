#pragma once
#include <Renderer/RenderCore/GpuResource.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/GpuTexture.h>
namespace ltn {
struct UpdateHeader {
	GpuResource* _sourceResource = nullptr;
	GpuResource* _dstResource = nullptr;
	u32 _sourceOffset = 0;
	u32 _dstOffset = 0;
	u32 _copySizeInByte = 0;
};

struct StagingUpdateHeader {
	GpuResource* _dstResource = nullptr;
	u32 _dstOffset = 0;
	u32 _copySizeInByte = 0;
	u32 _stagingBufferOffset = 0;
};

struct TextureUpdateHeader {
	static constexpr u32 SUBRESOURCE_COUNT_MAX = 128;
	GpuTexture* _dstTexture = nullptr;
	u32 _firstSubResources = 0;
	u32 _numSubresources = 0;
	u32 _stagingBufferOffset = 0;
};

struct TextureCopyHeader {
	GpuTexture* _dstTexture = nullptr;
	GpuTexture _srcTexture;
	void* _srcTextureUniqueMarker = nullptr; // src テクスチャ識別子
	u32 _srcSubResourceIndex = 0;
	u32 _dstSubResourceIndex = 0;
};

class VramUpdater {
public:
	static constexpr u32 STAGING_BUFFER_SIZE_IN_BYTE = 1024 * 1024 * 128; // 128MB
	static constexpr u32 BUFFER_HEADER_COUNT_MAX = 1024 * 4;
	static constexpr u32 TEXTURE_HEADER_COUNT_MAX = 512;

	void initialize();
	void terminate();
	void update(u32 frameIndex);

	void enqueueUpdate(GpuResource* dstBuffer, u32 dstOffset, GpuResource* sourceBuffer, u32 sourceOffset, u32 copySizeInByte);
	void enqueueUpdate(GpuTexture* dstTexture, GpuTexture* srcTexture, u32 srcSubResourceIndex, u32 dstSubResourceIndex);
	void* enqueueUpdate(GpuResource* dstBuffer, u32 dstOffset, u32 copySizeInByte);
	void* enqueueUpdate(GpuTexture* dstTexture, u32 firstSubResourceIndex, u32 subResourceCount, const rhi::SubResourceData* subResources, u32 copySizeInByte);

	template<class T>
	T* enqueueUpdate(GpuResource* dstBuffer, u32 numOffset = 0, u32 numElements = 1) {
		return reinterpret_cast<T*>(enqueueUpdate(dstBuffer, sizeof(T) * numOffset, sizeof(T) * numElements));
	}

	void populateCommandList(rhi::CommandList* commandList);

	static VramUpdater* Get();

private:
	void* allocateUpdateBuffer(u32 sizeInByte, u32 alignment);
	u32 getStagingBufferStartOffset(const void* allocatedPtr) const;

private:
	static constexpr u32 OVER_RUN_MARKER = 0xfbfbfbfbU;
	UpdateHeader _updateHeaders[BUFFER_HEADER_COUNT_MAX] = {};
	StagingUpdateHeader _stagingUpdateHeaders[BUFFER_HEADER_COUNT_MAX] = {};
	TextureUpdateHeader _textureUpdateHeaders[TEXTURE_HEADER_COUNT_MAX] = {};
	TextureCopyHeader _textureCopyHeaders[TEXTURE_HEADER_COUNT_MAX] = {};
	GpuBuffer _stagingBuffer;
	u8* _stagingMapPtr = nullptr;
	u32 _frameStarts[rhi::BACK_BUFFER_COUNT] = {};
	u32 _prevBufferStart = 0;
	u32 _currentBufferStart = 0;
	u32 _updateHeaderCount = 0;
	u32 _stagingUpdateHeaderCount = 0;
	u32 _textureUpdateHeaderCount = 0;
	u32 _textureCopyHeaderCount = 0;
	u32 _uploadSizeInByte = 0;
};
}