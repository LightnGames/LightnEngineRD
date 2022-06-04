#include "Shader.h"
#include <Core/Memory.h>

namespace ltn {
namespace {
ShaderScene g_shaderScene;
}
void ShaderScene::initialize() {
	VirtualArray::Desc handleDesc = {};
	handleDesc._size = SHADER_COUNT_MAX;
	_shaderAllocations.initialize(handleDesc);

	_shaders = Memory::allocObjects<Shader>(SHADER_COUNT_MAX);
	_shaderAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(SHADER_COUNT_MAX);
	_shaderAssetPathHashes = Memory::allocObjects<u64>(SHADER_COUNT_MAX);
	_shaderAssetPaths = Memory::allocObjects<char*>(SHADER_COUNT_MAX);
}

void ShaderScene::terminate() {
	_shaderAllocations.terminate();
	Memory::freeObjects(_shaders);
	Memory::freeObjects(_shaderAllocationInfos);
	Memory::freeObjects(_shaderAssetPathHashes);
}

void ShaderScene::lateUpdate() {
	u32 destroyShaderCount = _shaderDestroyInfos.getUpdateCount();
	auto destroyShaders = _shaderDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyShaderCount; ++i) {
		u32 shaderIndex = getShaderIndex(destroyShaders[i]);
		_shaderAllocations.freeAllocation(_shaderAllocationInfos[shaderIndex]);
		Memory::freeObjects(_shaderAssetPaths[i]);
	}

	_shaderCreateInfos.reset();
	_shaderDestroyInfos.reset();
}

Shader* ShaderScene::createShader(const CreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _shaderAllocations.allocation(1);

	_shaderAllocationInfos[allocationInfo._offset] = allocationInfo;
	u32 assetPathLength = StrLength(desc._assetPath) + 1;

	char*& shaderAssetPath = _shaderAssetPaths[allocationInfo._offset];
	shaderAssetPath = Memory::allocObjects<char>(assetPathLength);
	memcpy(shaderAssetPath, desc._assetPath, assetPathLength);

	Shader* shader = &_shaders[allocationInfo._offset];
	shader->_assetPath = _shaderAssetPaths[allocationInfo._offset];

	_shaderAssetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);
	_shaderCreateInfos.push(shader);
	return shader;
}

void ShaderScene::destroyShader(Shader* shader) {
	_shaderDestroyInfos.push(shader);
}

Shader* ShaderScene::findShader(u64 assetPathHash) {
	for (u32 i = 0; i < SHADER_COUNT_MAX; ++i) {
		if (_shaderAssetPathHashes[i] == assetPathHash) {
			return &_shaders[i];
		}
	}
	return nullptr;
}
ShaderScene* ShaderScene::Get() {
	return &g_shaderScene;
}
}
