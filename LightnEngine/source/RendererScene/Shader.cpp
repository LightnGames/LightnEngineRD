#include "Shader.h"
#include <Core/Memory.h>

namespace ltn {
namespace {
ShaderScene g_shaderScene;
}
void ShaderScene::initialize() {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = SHADER_CAPACITY;
		_shaderAllocations.initialize(handleDesc);

		handleDesc._size = SHADER_PARAMETER_CAPACITY;
		_parameterAllocations.initialize(handleDesc);
	}

	_shaders = Memory::allocObjects<Shader>(SHADER_CAPACITY);
	_shaderAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(SHADER_CAPACITY);
	_shaderAssetPathHashes = Memory::allocObjects<u64>(SHADER_CAPACITY);
	_shaderAssetPaths = Memory::allocObjects<char*>(SHADER_CAPACITY);
	_parameterAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(SHADER_PARAMETER_CAPACITY);
	_parameterNameHashes = Memory::allocObjects<u32>(SHADER_PARAMETER_CAPACITY);
	_parameterOffsets = Memory::allocObjects<u16>(SHADER_PARAMETER_CAPACITY);
}

void ShaderScene::terminate() {
	_shaderAllocations.terminate();
	_parameterAllocations.terminate();
	Memory::freeObjects(_shaders);
	Memory::freeObjects(_shaderAllocationInfos);
	Memory::freeObjects(_shaderAssetPathHashes);
	Memory::freeObjects(_parameterNameHashes);
	Memory::freeObjects(_parameterOffsets);
}

void ShaderScene::lateUpdate() {
	u32 destroyShaderCount = _shaderDestroyInfos.getUpdateCount();
	auto destroyShaders = _shaderDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyShaderCount; ++i) {
		u32 shaderIndex = getShaderIndex(destroyShaders[i]);
		_shaderAllocations.freeAllocation(_shaderAllocationInfos[shaderIndex]);
		Memory::freeObjects(_shaderAssetPaths[i]);

		if (destroyShaders[i]->_parameterSizeInByte != 0) {
			_parameterAllocations.freeAllocation(_parameterAllocationInfos[shaderIndex]);
		}

		_shaders[shaderIndex] = Shader();
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
	shader->_assetPathHash = StrHash64(desc._assetPath);

	// シェーダーパラメーター情報
	if (desc._TEST_collectParameter) {
		u32 parameterCount = 2;
		u32 parameterSizeInByte = 20;
		VirtualArray::AllocationInfo parameterAllocationInfo = _parameterAllocations.allocation(parameterCount);
		_parameterAllocationInfos[allocationInfo._offset] = parameterAllocationInfo;

		u32* parameterNameHashes = &_parameterNameHashes[parameterAllocationInfo._offset];
		u16* parameterOffsets = &_parameterOffsets[parameterAllocationInfo._offset];
		shader->_parameterCount = parameterCount;
		shader->_parameterNameHashes = parameterNameHashes;
		shader->_parameterOffsets = parameterOffsets;
		shader->_parameterSizeInByte = parameterSizeInByte;

		// offset:0 size:16 BaseColor
		parameterNameHashes[0] = StrHash32("BaseColor");
		parameterOffsets[0] = 0;

		// offset:16 size:4 BaseColorTextureIndex
		parameterNameHashes[1] = StrHash32("BaseColorTexture");
		parameterOffsets[1] = 16;
	}

	_shaderAssetPathHashes[allocationInfo._offset] = shader->_assetPathHash;
	_shaderCreateInfos.push(shader);
	return shader;
}

void ShaderScene::destroyShader(Shader* shader) {
	_shaderDestroyInfos.push(shader);
}

Shader* ShaderScene::findShader(u64 assetPathHash) {
	for (u32 i = 0; i < SHADER_CAPACITY; ++i) {
		if (_shaderAssetPathHashes[i] == assetPathHash) {
			return &_shaders[i];
		}
	}
	return nullptr;
}
ShaderScene* ShaderScene::Get() {
	return &g_shaderScene;
}
bool Shader::findParameter(u32 nameHash, u16& outOffsetSizeInByte) const {
	for (u16 i = 0; i < _parameterCount; ++i) {
		if (_parameterNameHashes[i] == nameHash) {
			outOffsetSizeInByte = _parameterOffsets[i];
			return true;
		}
	}

	return false;
}
}
