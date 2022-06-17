#include "MeshPreset.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Material.h>

namespace ltn {
namespace {
MeshPresetScene g_meshPresetScene;
}

void MeshPresetScene::initialize() {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = MESH_PRESET_CAPACITY;
		_meshPresetAllocations.initialize(handleDesc);
		_materialAllocations.initialize(handleDesc);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_assetPathHashes = allocation.allocateObjects<u64>(MESH_PRESET_CAPACITY);
		_meshPresets = allocation.allocateObjects<MeshPreset>(MESH_PRESET_CAPACITY);
		_meshPresetAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(MESH_PRESET_CAPACITY);
		_materials = allocation.allocateObjects<Material*>(MESH_PRESET_CAPACITY);
		_materialAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(MESH_PRESET_CAPACITY);
		});
}

void MeshPresetScene::terminate() {
	_meshPresetAllocations.terminate();
	_materialAllocations.terminate();
	_chunkAllocator.free();
}

const MeshPreset* MeshPresetScene::createMeshPreset(const MeshPresetCreatationDesc& desc) {
	u64 materialSlotNameHashes[Mesh::MATERIAL_SLOT_COUNT_MAX];
	u64 materialPathHashes[Mesh::MATERIAL_SLOT_COUNT_MAX];
	u64 meshPathHash = 0;
	u32 materialSlotCount = 0;

	{
		AssetPath meshAsset(desc._assetPath);
		meshAsset.openFile();
		meshAsset.readFile(&meshPathHash, sizeof(u64));
		meshAsset.readFile(&materialSlotCount, sizeof(u32));
		meshAsset.readFile(&materialSlotNameHashes, sizeof(u64) * materialSlotCount);
		meshAsset.readFile(&materialPathHashes, sizeof(u64) * materialSlotCount);
		meshAsset.closeFile();
	}

	LTN_ASSERT(materialSlotCount < Mesh::MATERIAL_SLOT_COUNT_MAX);


	VirtualArray::AllocationInfo meshPresetAllocationInfo = _meshPresetAllocations.allocation(1);
	VirtualArray::AllocationInfo materialAllocationInfo = _materialAllocations.allocation(materialSlotCount);
	Material** materials = &_materials[materialAllocationInfo._offset];
	MeshPreset* meshPreset = &_meshPresets[meshPresetAllocationInfo._offset];
	meshPreset->setMesh(MeshScene::Get()->findMesh(meshPathHash));
	meshPreset->setMaterials(materials);

	MaterialScene* materialScene = MaterialScene::Get();
	for (u32 i = 0; i < materialSlotCount; ++i) {
		u16 materialSlotIndex = meshPreset->getMesh()->findMaterialSlotIndex(materialSlotNameHashes[i]);
		LTN_ASSERT(materialSlotIndex != Mesh::INVALID_MATERIAL_SLOT_INDEX);
		materials[materialSlotIndex] = materialScene->findMaterial(materialPathHashes[i]);
		LTN_ASSERT(materials[materialSlotIndex] != nullptr);
	}

	_assetPathHashes[meshPresetAllocationInfo._offset] = StrHash64(desc._assetPath);
	_meshPresetAllocationInfos[meshPresetAllocationInfo._offset] = meshPresetAllocationInfo;
	_materialAllocationInfos[meshPresetAllocationInfo._offset] = materialAllocationInfo;
	return meshPreset;
}

void MeshPresetScene::destroyMeshPreset(const MeshPreset* meshPreset) {
	u32 meshPresetIndex = getMeshPresetIndex(meshPreset);
	_meshPresetAllocations.freeAllocation(_meshPresetAllocationInfos[meshPresetIndex]);
	_materialAllocations.freeAllocation(_materialAllocationInfos[meshPresetIndex]);
}

MeshInstance* MeshPreset::createMeshInstance(u32 instanceCount) const {
	MeshInstanceScene::CreatationDesc desc;
	desc._mesh = _mesh;
	MeshInstance* meshInstance = MeshInstanceScene::Get()->createMeshInstance(desc);

	u16 materialSlotCount = _mesh->getMaterialSlotCount();
	for (u16 i = 0; i < materialSlotCount; ++i) {
		meshInstance->setMaterial(i, _materials[i]);
	}

	return meshInstance;
}

const MeshPreset* MeshPresetScene::findMeshPreset(u64 assetPathHash) const {
	for (u32 i = 0; i < MESH_PRESET_CAPACITY; ++i) {
		if (_assetPathHashes[i] == assetPathHash) {
			return &_meshPresets[i];
		}
	}

	return nullptr;
}

MeshPresetScene* MeshPresetScene::Get() {
	return &g_meshPresetScene;
}
}
