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

	rhi::GpuDescriptorHandle getTextureGpuSrv() const { return _textureSrv._firstHandle._gpuHandle; }

	static GpuTextureManager* Get();


private:
	void createTexture(const Texture* texture, u32 beginMipOffset, u32 loadMipCount);
	void createEmptyTexture(GpuTexture& gpuTexture, u32 width, u32 height, rhi::Format format);
	void GpuTextureManager::copyTexture(GpuTexture* dstTexture, GpuTexture* srcTexture, u32 firstSubResource, u32 subResourceCount, s32 srcSubResourceOffset, s32 dstSubResourceOffset);

private:
	DescriptorHandles _textureSrv;
	GpuTexture* _textures = nullptr;
	u8* _textureEnabledFlags = nullptr;
};
}