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

	rhi::GpuDescriptorHandle getTextureGpuSrv() const { return _textureSrv._firstHandle._gpuHandle; }

	static GpuTextureManager* Get();

private:
	void createTexture(const Texture* texture);

private:
	DescriptorHandles _textureSrv;
	GpuTexture* _textures = nullptr;
};
}