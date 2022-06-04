#include "GpuShader.h"
#include <Core/Memory.h>
#include <RendererScene/Shader.h>

namespace ltn {
namespace {
GpuShaderScene g_gpuShaderScene;
}

void GpuShaderScene::initialize() {
	_shaders = Memory::allocObjects<rhi::ShaderBlob>(ShaderScene::SHADER_COUNT_MAX);
}

void GpuShaderScene::terminate() {
	Memory::freeObjects(_shaders);
}

void GpuShaderScene::update() {
	ShaderScene* shaderScene = ShaderScene::Get();
	const UpdateInfos<Shader>* shaderCreateInfos = shaderScene->getCreateInfos();
	u32 createCount = shaderCreateInfos->getUpdateCount();
	auto createShaders = shaderCreateInfos->getObjects();
	for (u32 i = 0; i < createCount; ++i) {
		const Shader* shader = createShaders[i];
		AssetPath shaderPath(shader->_assetPath);

		u32 shaderIndex = shaderScene->getShaderIndex(shader);
		rhi::ShaderBlob& shaderBlob = _shaders[shaderIndex];
		shaderBlob.initialize(shaderPath.get());
	}

	const UpdateInfos<Shader>* shaderDestroyInfos = shaderScene->getDestroyInfos();
	u32 destroyCount = shaderDestroyInfos->getUpdateCount();
	auto destroyShaders = shaderDestroyInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		const Shader* shader = destroyShaders[i];
		u32 shaderIndex = shaderScene->getShaderIndex(shader);
		rhi::ShaderBlob& shaderBlob = _shaders[shaderIndex];
		shaderBlob.terminate();
	}
}

GpuShaderScene* GpuShaderScene::Get() {
	return &g_gpuShaderScene;
}
}
