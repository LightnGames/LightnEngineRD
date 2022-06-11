#pragma once
#include <Renderer/RHI/Rhi.h>
namespace ltn {
class Shader;
class GpuShaderScene {
public:
	void initialize();
	void terminate();
	void update();

	rhi::ShaderBlob* getShader(u32 index) { return &_shaders[index]; }

	void initializeShader(const Shader* shader);
	void terminateShader(const Shader* shader);

	static GpuShaderScene* Get();

private:
	rhi::ShaderBlob* _shaders = nullptr;
};
}