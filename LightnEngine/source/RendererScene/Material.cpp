#include "Material.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <Core/Math.h>
#include <Core/CpuTimerManager.h>
#include <RendererScene/Shader.h>
#include <RendererScene/Texture.h>

namespace ltn {
namespace {
MaterialScene g_materialScene;
MaterialParameterContainer g_materialParameterContainer;
PipelineSetScene g_pipelineSetScene;
}

void MaterialScene::initialize() {
	CpuScopedPerf scopedPerf("MaterialScene");
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = MATERIAL_CAPACITY;
		_materialAllocations.initialize(handleDesc);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_materials = allocation.allocateObjects<Material>(MATERIAL_CAPACITY);
		_materialAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(MATERIAL_CAPACITY);
		_materialAssetPathHashes = allocation.allocateObjects<u64>(MATERIAL_CAPACITY);
		_enabledFlags = allocation.allocateClearedObjects<u8>(MATERIAL_CAPACITY);
		_assetPaths = allocation.allocateObjects<char*>(MATERIAL_CAPACITY);
		});

	MaterialParameterContainer::Get()->initialize();
	PipelineSetScene::Get()->initialize();
}

void MaterialScene::terminate() {
	lateUpdate();

	_materialAllocations.terminate();
	_chunkAllocator.free();

	MaterialParameterContainer::Get()->terminate();
	PipelineSetScene::Get()->terminate();
}

void MaterialScene::lateUpdate() {
	PipelineSetScene::Get()->lateUpdate();
	MaterialParameterContainer::Get()->lateUpdate();

#define ENABLE_ZERO_CLEAR 1
	u32 destroyMeshCount = _materialDestroyInfos.getUpdateCount();
	auto destroyMaterials = _materialDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		u32 materialIndex = getMaterialIndex(destroyMaterials[i]);
		LTN_ASSERT(materialIndex < MATERIAL_CAPACITY);
		LTN_ASSERT(_enabledFlags[materialIndex] == 1);
		_materialAllocations.freeAllocation(_materialAllocationInfos[materialIndex]);
		_enabledFlags[materialIndex] = 0;

		Memory::deallocObjects(_assetPaths[materialIndex]);
		MaterialParameterContainer::Get()->freeMaterialParameters(destroyMaterials[i]->getParameterSet());

#if ENABLE_ZERO_CLEAR
		_materialAllocationInfos[materialIndex] = VirtualArray::AllocationInfo();
		_materials[materialIndex] = Material();
		_materialAssetPathHashes[materialIndex] = 0;
#endif
	}

	_materialCreateInfos.reset();
	_materialDestroyInfos.reset();
}

const Material* MaterialScene::createMaterial(const MaterialCreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _materialAllocations.allocation(1);

	_materialAllocationInfos[allocationInfo._offset] = allocationInfo;
	Material* material = &_materials[allocationInfo._offset];

	u64 pipelineSetHash = 0;
	u32 materialParameterCount = 0;
	u32 materialParameterSize = 0;
	u8 materialParameters[256];
	{
		AssetPath asset(desc._assetPath);
		asset.openFile();
		asset.readFile(&pipelineSetHash, sizeof(u64));
		asset.readFile(&materialParameterCount, sizeof(u32));
		asset.readFile(&materialParameterSize, sizeof(u32));
		asset.readFile(materialParameters, materialParameterSize);
		asset.closeFile();
	}

	LTN_ASSERT(materialParameterSize < LTN_COUNTOF(materialParameters));

	u32 assetPathLength = StrLength(desc._assetPath) + 1;
	char*& assetPath = _assetPaths[allocationInfo._offset];
	assetPath = Memory::allocObjects<char>(assetPathLength);
	memcpy(assetPath, desc._assetPath, assetPathLength);
	material->setAssetPath(assetPath);

	const PipelineSet* pipelineSet = PipelineSetScene::Get()->findPipelineSet(pipelineSetHash);
	LTN_ASSERT(pipelineSet != nullptr);
	material->setPipelineSet(pipelineSet);

	// パラメーターメモリ確保
	u16 materialParameterSizeInByte = pipelineSet->getParameterSizeInByte();
	MaterialParameterSet materialParameterSet = MaterialParameterContainer::Get()->allocateMaterialParameters(materialParameterSizeInByte);
	material->setParameterSet(materialParameterSet);

	// マテリアルパラメーターバイナリを解析
	TextureScene* textureScene = TextureScene::Get();
	const u8* materialParameterReadPtr = materialParameters;
	for (u32 i = 0; i < materialParameterCount; ++i) {
		u32 parameterNameHash = *reinterpret_cast<const u32*>(materialParameterReadPtr);
		materialParameterReadPtr += sizeof(u32);

		u8 parameterType = *materialParameterReadPtr;
		LTN_ASSERT(parameterType < Shader::PARAMETER_TYPE_COUNT);
		materialParameterReadPtr += sizeof(u8);

		u32 writeParameterSize = 0;
		u8 readParameterSize = 0;
		u8* writePtr = materialParameterSet._parameters + pipelineSet->findMaterialParameterOffset(parameterNameHash);
		switch (parameterType) {
		case Shader::PARAMETER_TYPE_FLOAT4:
			writeParameterSize = sizeof(Float4);
			readParameterSize = sizeof(Float4);
			memcpy(writePtr, materialParameterReadPtr, writeParameterSize);
			break;

		case Shader::PARAMETER_TYPE_TEXTURE:
			writeParameterSize = sizeof(u32);
			readParameterSize = sizeof(u64);
			u64 textureAssetPathHash;
			memcpy(&textureAssetPathHash, materialParameterReadPtr, readParameterSize);

			const Texture* texture = textureScene->findTexture(textureAssetPathHash);
			u32 textureIndex = textureScene->getTextureIndex(texture);
			memcpy(writePtr, &textureIndex, writeParameterSize);
			break;
		}
		materialParameterReadPtr += readParameterSize;
	}

	_enabledFlags[allocationInfo._offset] = 1;
	_materialAssetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);

	MaterialParameterContainer::Get()->postUpdateMaterialParameter(material->getParameterSet());
	_materialCreateInfos.push(material);
	return material;
}

void MaterialScene::createMaterials(const MaterialCreatationDesc* descs, Material const** materials, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		const MaterialCreatationDesc* desc = descs + i;
		materials[i] = createMaterial(*desc);
	}
}

void MaterialScene::destroyMaterial(const Material* material) {
	_materialDestroyInfos.push(material);
}

void MaterialScene::destroyMaterials(Material const** materials, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		destroyMaterial(materials[i]);
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

MaterialScene* MaterialScene::Get() {
	return &g_materialScene;
}
u32 PipelineSet::getParameterSizeInByte() const {
	return _vertexShader->_parameterSizeInByte + _pixelShader->_parameterSizeInByte;
}

u16 PipelineSet::findMaterialParameterOffset(u32 parameterNameHash) const {
	u16 offset;
	if (getVertexShader()->findParameter(parameterNameHash, offset)) {
		return offset;
	}

	if (getPixelShader()->findParameter(parameterNameHash, offset)) {
		return offset;
	}

	LTN_ASSERT(false);
	return 0;
}
u16 PipelineSet::findMaterialParameters(u8 parameterType, u16* outParameterTypes) const {
	u16 parameterCount = 0;
	parameterCount += _vertexShader->findParameters(parameterType, outParameterTypes);
	parameterCount += _pixelShader->findParameters(parameterType, outParameterTypes + parameterCount);
	return parameterCount;
}

void MaterialParameterContainer::initialize() {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = MATERIAL_PARAMETER_SIZE_IN_BYTE;
		_materialParameterAllocations.initialize(handleDesc);
	}

	_materialParameters = Memory::allocObjects<u8>(MATERIAL_PARAMETER_SIZE_IN_BYTE);
}

void MaterialParameterContainer::terminate() {
	_materialParameterAllocations.terminate();
	Memory::deallocObjects(_materialParameters);
}

void MaterialParameterContainer::lateUpdate() {
	_materialParameterUpdateInfos.reset();
}

MaterialParameterSet MaterialParameterContainer::allocateMaterialParameters(u32 sizeInByte) {
	VirtualArray::AllocationInfo allocationInfo = _materialParameterAllocations.allocation(sizeInByte);
	MaterialParameterSet parameterSet;
	parameterSet._parameters = &_materialParameters[allocationInfo._offset];
	parameterSet._sizeInByte = sizeInByte;
	parameterSet._allocationInfo = allocationInfo;
	return parameterSet;
}

void MaterialParameterContainer::freeMaterialParameters(const MaterialParameterSet* parameterSet) {
	_materialParameterAllocations.freeAllocation(parameterSet->_allocationInfo);
}

MaterialParameterContainer* MaterialParameterContainer::Get() {
	return &g_materialParameterContainer;
}

void PipelineSetScene::initialize() {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = PIPELINE_SET_CAPACITY;
		_pipelineSetAllocations.initialize(handleDesc);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_pipelineSets = allocation.allocateObjects<PipelineSet>(PIPELINE_SET_CAPACITY);
		_pipelineSetAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(PIPELINE_SET_CAPACITY);
		_assetPathHashes = allocation.allocateObjects<u64>(PIPELINE_SET_CAPACITY);
		_enabledFlags = allocation.allocateObjects<u8>(PIPELINE_SET_CAPACITY);
		});

	memset(_enabledFlags, 0, PIPELINE_SET_CAPACITY);
}

void PipelineSetScene::terminate() {
	lateUpdate();

	_pipelineSetAllocations.terminate();
	_chunkAllocator.free();
}

void PipelineSetScene::lateUpdate() {
#define ENABLE_ZERO_CLEAR 1
	u32 destroyCount = _pipelineSetDestroyInfos.getUpdateCount();
	auto destroyPipelineSets = _pipelineSetDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		u32 pipelineSetIndex = getPipelineSetIndex(destroyPipelineSets[i]);
		LTN_ASSERT(pipelineSetIndex < PIPELINE_SET_CAPACITY);
		_pipelineSetAllocations.freeAllocation(_pipelineSetAllocationInfos[pipelineSetIndex]);
		_enabledFlags[pipelineSetIndex] = 0;

#if ENABLE_ZERO_CLEAR
		_pipelineSetAllocationInfos[pipelineSetIndex] = VirtualArray::AllocationInfo();
		_pipelineSets[pipelineSetIndex] = PipelineSet();
		_assetPathHashes[pipelineSetIndex] = 0;
#endif
	}

	_pipelineSetCreateInfos.reset();
	_pipelineSetDestroyInfos.reset();
}

const PipelineSet* PipelineSetScene::createPipelineSet(const CreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _pipelineSetAllocations.allocation(1);
	_pipelineSetAllocationInfos[allocationInfo._offset] = allocationInfo;

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
	const Shader* vertexShader = shaderScene->findShader(vertexShaderHash);
	const Shader* pixelShader = shaderScene->findShader(pixelShaderHash);
	LTN_ASSERT(vertexShader != nullptr);
	LTN_ASSERT(pixelShader != nullptr);

	PipelineSet* pipelineSet = &_pipelineSets[allocationInfo._offset];
	pipelineSet->_vertexShader = vertexShader;
	pipelineSet->_pixelShader = pixelShader;

	_enabledFlags[allocationInfo._offset] = 1;
	_assetPathHashes[allocationInfo._offset] = StrHash64(desc._assetPath);
	_pipelineSetCreateInfos.push(pipelineSet);
	return pipelineSet;
}

void PipelineSetScene::destroyPipelineSet(const PipelineSet* pipelineSet) {
	_pipelineSetDestroyInfos.push(pipelineSet);
}

const PipelineSet* PipelineSetScene::findPipelineSet(u64 pipelineDescHash) const {
	for (u32 i = 0; i < PIPELINE_SET_CAPACITY; ++i) {
		if (_assetPathHashes[i] == pipelineDescHash) {
			return &_pipelineSets[i];
		}
	}

	return nullptr;
}

PipelineSetScene* PipelineSetScene::Get() {
	return &g_pipelineSetScene;
}
}
