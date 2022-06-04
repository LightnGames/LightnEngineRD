#pragma once
#include <Renderer/RHI/Rhi.h>
namespace ltn {
class GpuShaderScene {
public:
	void initialize();
	void terminate();
	void update();

	rhi::ShaderBlob* getShader(u32 index) { return &_shaders[index]; }

	static GpuShaderScene* Get();

private:
	rhi::ShaderBlob* _shaders = nullptr;
};
}