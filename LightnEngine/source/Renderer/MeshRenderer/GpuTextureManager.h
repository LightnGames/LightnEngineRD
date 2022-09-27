#pragma once
#include <Renderer/RenderCore/GpuTexture.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
namespace ltn {
class Texture;
class GpuTextureManager {
public:
	void initialize();
	void terminate();
	void update();

	void streamTexture(const Texture* texture, u32 currentStreamMipLevel, u32 targetStreamMipLevel);

	// テクスチャを生成した直後に SRV を生成するとデータコピー完了まで真っ黒なテクスチャになるので遅延更新リストに登録
	void requestCreateSrv(u32 textureIndex);

	rhi::GpuDescriptorHandle getTextureGpuSrv() const { return _textureSrv._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getTextureGpuSrv(u32 index) const { return _textureSrv.get(index)._gpuHandle; }
	const GpuTexture* getTexture(u32 index) const { return &_textures[index]; }

	static GpuTextureManager* Get();


private:
	void createTextureSrv(const Texture* texture);
	void createTexture(const Texture* texture, u32 beginMipOffset, u32 loadMipCount);
	void createEmptyTexture(GpuTexture& gpuTexture, u32 width, u32 height, u32 depth, rhi::Format format);
	void GpuTextureManager::copyTexture(GpuTexture* dstTexture, GpuTexture* srcTexture, u32 firstSubResource, u32 subResourceCount, s32 srcSubResourceOffset, s32 dstSubResourceOffset);

private:
	DescriptorHandles _textureSrv;
	GpuTexture* _textures = nullptr;
	u8* _textureEnabledFlags = nullptr;

	// SRV 遅延更新用情報
	static constexpr u32 REQUESTED_CREATE_SRV_CAPACITY = 128;
	static constexpr u32 REQUESTED_INVALID_INDEX = UINT32_MAX;
	u32 _requestedCreateSrvIndices[REQUESTED_CREATE_SRV_CAPACITY] = {};
	u8 _requestedCreateSrvTimers[REQUESTED_CREATE_SRV_CAPACITY] = {};
	GpuTexture _defaultBlackTexture;
};
}