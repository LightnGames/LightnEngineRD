#include "Material.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <Core/Math.h>
#include <RendererScene/Shader.h>

namespace ltn {
namespace {
MaterialScene g_materialScene;
}
void MaterialScene::initialize() {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = MATERIAL_CAPACITY;
		_materialAllocations.initialize(handleDesc);

		handleDesc._size = MATERIAL_INSTANCE_CAPACITY;
		_materialInstanceAllocations.initialize(handleDesc);

		handleDesc._size = MATERIAL_PARAMETER_SIZE_IN_BYTE;
		_materialParameterAllocations.initialize(handleDesc);
	}

	_materials = Memory::allocObjects<Material>(MATERIAL_CAPACITY);
	_materialInstances = Memory::allocObjects<MaterialInstance>(MATERIAL_INSTANCE_CAPACITY);
	_materialParameters = Memory::allocObjects<u8>(MATERIAL_PARAMETER_SIZE_IN_BYTE);
	_materialAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(MATERIAL_CAPACITY);
	_materialInstanceAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(MATERIAL_INSTANCE_CAPACITY);
	_materialParameterAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(MATERIAL_PARAMETER_SIZE_IN_BYTE);
	_enabledFlags = Memory::allocObjects<u8>(MATERIAL_CAPACITY);
	_materialAssetPathHashes = Memory::allocObjects<u64>(MATERIAL_CAPACITY);
	_materialInstanceAssetPathHashes = Memory::allocObjects<u64>(MATERIAL_INSTANCE_CAPACITY);

	memset(_enabledFlags, 0, MATERIAL_CAPACITY);
	memset(_materialParameterAllocationInfos, 0, sizeof(VirtualArray::AllocationInfo) * MATERIAL_INSTANCE_CAPACITY);
}

void MaterialScene::terminate() {
	lateUpdate();

	_materialAllocations.terminate();
	_materialInstanceAllocations.terminate();
	_materialParameterAllocations.terminate();

	Memory::freeObjects(_materials);
	Memory::freeObjects(_materialInstances);
	Memory::freeObjects(_materialParameters);
	Memory::freeObjects(_materialAllocationInfos);
	Memory::freeObjects(_materialInstanceAllocationInfos);
	Memory::freeObjects(_materialParameterAllocationInfos);
	Memory::freeObjects(_enabledFlags);
	Memory::freeObjects(_materialAssetPathHashes);
	Memory::freeObjects(_materialInstanceAssetPathHashes);
}

void MaterialScene::lateUpdate() {
#define ENABLE_ZERO_CLEAR 1
	u32 destroyMeshCount = _materialDestroyInfos.getUpdateCount();
	auto destroyMaterials = _materialDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		u32 materialIndex = getMaterialIndex(destroyMaterials[i]);
		LTN_ASSERT(materialIndex < MATERIAL_CAPACITY);
		_materialAllocations.freeAllocation(_materialAllocationInfos[materialIndex]);
		_enabledFlags[materialIndex] = 0;

#if ENABLE_ZERO_CLEAR
		_materialAllocationInfos[materialIndex] = VirtualArray::AllocationInfo();
		_materials[materialIndex] = Material();
		_materialAssetPathHashes[materialIndex] = 0;
#endif
	}

	_materialCreateInfos.reset();
	_materialDestroyInfos.reset();
	_materialInstanceUpdateInfos.reset();
}

Material* MaterialScene::createMaterial(const MaterialCreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _materialAllocations.allocation(1);

	_materialAllocationInfos[allocationInfo._offset] = allocationInfo;
	Material* material = &_materials[allocationInfo._offset];

	u64 vertexShaderHash = 0;
	u64 pixelShaderHash = 0;
	{
		AssetPath asset(desc._assetPath);
		asset.openFile();
		asset.readFile(&vertexShaderHash, sizeof(u64));
		asset.readFile(&pixelShaderHash, sizeof(u64));
		asset.closeFile();
	}

	ShaderScene* shaderScene = ShaderScene::Get();
	material->_vertexShader = shaderScene->findShader(vertexShaderHash);
	material->_pixelShader = shaderScene->findShader(pixelShaderHash);
	LTN_ASSERT(material->_vertexShader != nullptr);
	LTN_ASSERT(material->_pixelShader != nullptr);

	_materialAssetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);
	_enabledFlags[allocationInfo._offset] = 1;
	_materialCreateInfos.push(material);
	return material;
}

MaterialInstance* MaterialScene::createMaterialInstance(const MaterialInstanceCreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _materialInstanceAllocations.allocation(1);

	_materialInstanceAllocationInfos[allocationInfo._offset] = allocationInfo;
	MaterialInstance* materialInstance = &_materialInstances[allocationInfo._offset];

	u64 materialPathHash = 0;
	u32 materialParameterCount = 0;
	u32 materialParameterSize = 0;
	u8 materialParameters[256];
	{
		AssetPath asset(desc._assetPath);
		asset.openFile();
		asset.readFile(&materialPathHash, sizeof(u64));
		asset.readFile(&materialParameterCount, sizeof(u32));
		asset.readFile(&materialParameterSize, sizeof(u32));
		LTN_ASSERT(materialParameterSize < LTN_COUNTOF(materialParameters));
		asset.readFile(materialParameters, materialParameterSize);
		asset.closeFile();
	}

	const Material* material = findMaterial(materialPathHash);
	u16 materialParameterSizeInByte = 0;
	materialParameterSizeInByte += material->getVertexShader()->_parameterSizeInByte;
	materialParameterSizeInByte += material->getPixelShader()->_parameterSizeInByte;

	// マテリアルパラメーターがあればそのサイズ分確保
	if (materialParameterSizeInByte > 0) {
		VirtualArray::AllocationInfo materialParameterAllocationInfo = _materialParameterAllocations.allocation(materialParameterSizeInByte);
		materialInstance->setMaterialParameters(&_materialParameters[materialParameterAllocationInfo._offset]);
		_materialParameterAllocationInfos[allocationInfo._offset] = materialParameterAllocationInfo;
	}
	materialInstance->setParentMaterial(material);

	// マテリアルパラメーターバイナリを解析
	const u8* materialParameterReadPtr = materialParameters;
	for (u32 i = 0; i < materialParameterCount; ++i) {
		u32 parameterNameHash = *reinterpret_cast<const u32*>(materialParameterReadPtr);
		materialParameterReadPtr += sizeof(u32);
		
		u8 parameterType = *materialParameterReadPtr;
		LTN_ASSERT(parameterType < Shader::PARAMETER_TYPE_COUNT);
		materialParameterReadPtr += sizeof(u8);

		u32 writeParameterSize = 0;
		u8 readParameterSize = 0;
		switch (parameterType) {
		case Shader::PARAMETER_TYPE_FLOAT4:
			writeParameterSize = sizeof(Float4);
			readParameterSize = sizeof(Float4);
			materialInstance->setParameter(parameterNameHash, materialParameterReadPtr, writeParameterSize);
			break;

		case Shader::PARAMETER_TYPE_TEXTURE:
			u32 textureIndex = 0;
			writeParameterSize = sizeof(u32);
			readParameterSize = sizeof(u64);
			materialInstance->setParameter(parameterNameHash, &textureIndex, writeParameterSize);
			break;
		}
		materialParameterReadPtr += readParameterSize;
	}

	_materialInstanceAssetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);
	_materialInstanceUpdateInfos.push(materialInstance);
	return materialInstance;
}

void MaterialScene::destroyMaterial(Material* material) {
	_materialDestroyInfos.push(material);
}

void MaterialScene::destroyMaterialInstance(MaterialInstance* materialInstance) {
	u32 materialInstanceIndex = getMaterialInstanceIndex(materialInstance);
	LTN_ASSERT(materialInstanceIndex < MATERIAL_CAPACITY);
	_materialInstanceAllocations.freeAllocation(_materialInstanceAllocationInfos[materialInstanceIndex]);

	if (materialInstance->isEnabledMaterialParameter()) {
		_materialParameterAllocations.freeAllocation(_materialParameterAllocationInfos[materialInstanceIndex]);
	}

#define ENABLE_ZERO_CLEAR 1
#if ENABLE_ZERO_CLEAR
	_materialInstanceAllocationInfos[materialInstanceIndex] = VirtualArray::AllocationInfo();
	_materialParameterAllocationInfos[materialInstanceIndex] = VirtualArray::AllocationInfo();
	_materialInstances[materialInstanceIndex] = MaterialInstance();
#endif
}

void MaterialInstance::setParameter(u32 nameHash, const void* parameter, u32 sizeInByte) {
	u16 offset;
	if (_parentMaterial->getVertexShader()->findParameter(nameHash, offset)) {
		memcpy(&_materialParameters[offset], parameter, sizeInByte);
	}

	if (_parentMaterial->getPixelShader()->findParameter(nameHash, offset)) {
		memcpy(&_materialParameters[offset], parameter, sizeInByte);
	}
}

Material* MaterialScene::findMaterial(u64 assetPathHash) {
	for (u32 i = 0; i < MATERIAL_CAPACITY; ++i) {
		if (_materialAssetPathHashes[i] == assetPathHash) {
			return &_materials[i];
		}
	}
	return nullptr;
}

MaterialInstance* MaterialScene::findMaterialInstance(u64 assetPathHash) {
	for (u32 i = 0; i < MATERIAL_INSTANCE_CAPACITY; ++i) {
		if (_materialInstanceAssetPathHashes[i] == assetPathHash) {
			return &_materialInstances[i];
		}
	}
	return nullptr;
}

MaterialScene* MaterialScene::Get() {
	return &g_materialScene;
}
u32 Material::getParameterSizeInByte() const {
	return _vertexShader->_parameterSizeInByte + _pixelShader->_parameterSizeInByte;
}
}
