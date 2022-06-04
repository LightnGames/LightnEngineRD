#include "Material.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <RendererScene/Shader.h>

namespace ltn {
namespace {
MaterialScene g_materialScene;
}
void MaterialScene::initialize() {
	VirtualArray::Desc handleDesc = {};
	handleDesc._size = MATERIAL_COUNT_MAX;
	_materialAllocations.initialize(handleDesc);

	_materials = Memory::allocObjects<Material>(MATERIAL_COUNT_MAX);
	_materialAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(MATERIAL_COUNT_MAX);
	_enabledFlags = Memory::allocObjects<u8>(MATERIAL_COUNT_MAX);

	memset(_enabledFlags, 0, MATERIAL_COUNT_MAX);
}

void MaterialScene::terminate() {
	lateUpdate();
	_materialAllocations.terminate();
	Memory::freeObjects(_materials);
	Memory::freeObjects(_materialAllocationInfos);
	Memory::freeObjects(_enabledFlags);
}

void MaterialScene::lateUpdate() {
#define ENABLE_ZERO_CLEAR 1
	u32 destroyMeshCount = _materialDestroyInfos.getUpdateCount();
	auto destroyMaterials = _materialDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		u32 materialIndex = u32(destroyMaterials[i] - _materials);
		LTN_ASSERT(materialIndex < MATERIAL_COUNT_MAX);
		_materialAllocations.freeAllocation(_materialAllocationInfos[materialIndex]);
		_enabledFlags[materialIndex] = 0;

#if ENABLE_ZERO_CLEAR
		_materialAllocationInfos[materialIndex] = VirtualArray::AllocationInfo();
		_materials[materialIndex] = Material();
#endif
	}

	_materialCreateInfos.reset();
	_materialDestroyInfos.reset();
}

Material* MaterialScene::createMaterial(const CreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _materialAllocations.allocation(1);

	_materialAllocationInfos[allocationInfo._offset] = allocationInfo;
	Material* material = &_materials[allocationInfo._offset];

	u64 vertexShaderHash = 0;
	u64 pixelShaderHash = 0;
	{
		AssetPath meshAsset(desc._assetPath);
		meshAsset.openFile();
		meshAsset.readFile(&vertexShaderHash, sizeof(u64));
		meshAsset.readFile(&pixelShaderHash, sizeof(u64));
		meshAsset.closeFile();
	}

	ShaderScene* shaderScene = ShaderScene::Get();
	material->_vertexShader = shaderScene->findShader(vertexShaderHash);
	material->_pixelShader = shaderScene->findShader(pixelShaderHash);
	LTN_ASSERT(material->_vertexShader != nullptr);
	LTN_ASSERT(material->_pixelShader != nullptr);

	_enabledFlags[allocationInfo._offset] = 1;
	_materialCreateInfos.push(material);
	return material;
}

void MaterialScene::destroyMaterial(Material* material) {
	_materialDestroyInfos.push(material);
}

MaterialScene* MaterialScene::Get() {
	return &g_materialScene;
}
}
