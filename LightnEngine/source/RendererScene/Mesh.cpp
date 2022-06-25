#include "Mesh.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <Core/CpuTimerManager.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Material.h>

namespace ltn {
namespace {
MeshScene g_meshScene;
}

void MeshScene::initialize() {
	CpuScopedPerf scopedPerf("MeshScene");
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = MESH_CAPACITY;
		_meshAllocations.initialize(handleDesc);
		_materialAllocations.initialize(handleDesc);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_assetPathHashes = allocation.allocateObjects<u64>(MESH_CAPACITY);
		_meshes = allocation.allocateObjects<Mesh>(MESH_CAPACITY);
		_meshAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(MESH_CAPACITY);
		_materials = allocation.allocateObjects<Material*>(MESH_CAPACITY);
		_materialAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(MESH_CAPACITY);
		});
}

void MeshScene::terminate() {
	_meshAllocations.terminate();
	_materialAllocations.terminate();
	_chunkAllocator.free();
}

const Mesh* MeshScene::createMesh(const CreatationDesc& desc) {
	u64 materialSlotNameHashes[MeshGeometry::MATERIAL_SLOT_COUNT_MAX];
	u64 materialPathHashes[MeshGeometry::MATERIAL_SLOT_COUNT_MAX];
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

	LTN_ASSERT(materialSlotCount < MeshGeometry::MATERIAL_SLOT_COUNT_MAX);


	VirtualArray::AllocationInfo meshAllocationInfo = _meshAllocations.allocation(1);
	VirtualArray::AllocationInfo materialAllocationInfo = _materialAllocations.allocation(materialSlotCount);
	Material** materials = &_materials[materialAllocationInfo._offset];
	Mesh* mesh = &_meshes[meshAllocationInfo._offset];
	mesh->setMeshGeometry(MeshGeometryScene::Get()->findMeshGeometry(meshPathHash));
	mesh->setMaterials(materials);

	MaterialScene* materialScene = MaterialScene::Get();
	for (u32 i = 0; i < materialSlotCount; ++i) {
		u16 materialSlotIndex = mesh->getMeshGeometry()->findMaterialSlotIndex(materialSlotNameHashes[i]);
		LTN_ASSERT(materialSlotIndex != MeshGeometry::INVALID_MATERIAL_SLOT_INDEX);
		materials[materialSlotIndex] = materialScene->findMaterial(materialPathHashes[i]);
		LTN_ASSERT(materials[materialSlotIndex] != nullptr);
	}

	_assetPathHashes[meshAllocationInfo._offset] = StrHash64(desc._assetPath);
	_meshAllocationInfos[meshAllocationInfo._offset] = meshAllocationInfo;
	_materialAllocationInfos[meshAllocationInfo._offset] = materialAllocationInfo;
	return mesh;
}

void MeshScene::createMeshes(const CreatationDesc* descs, Mesh const** meshes, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		const CreatationDesc* desc = descs + i;
		meshes[i] = createMesh(*desc);
	}
}

void MeshScene::destroyMesh(const Mesh* mesh) {
	u32 meshIndex = getMeshIndex(mesh);
	_meshAllocations.freeAllocation(_meshAllocationInfos[meshIndex]);
	_materialAllocations.freeAllocation(_materialAllocationInfos[meshIndex]);
}

void MeshScene::destroyMeshes(Mesh const** meshes, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		destroyMesh(meshes[i]);
	}
}

MeshInstance* Mesh::createMeshInstances(u32 instanceCount) const {
	MeshInstanceScene::CreatationDesc desc;
	desc._mesh = _meshGeometry;
	MeshInstance* meshInstances = MeshInstanceScene::Get()->createMeshInstances(desc, instanceCount);

	u16 materialSlotCount = _meshGeometry->getMaterialSlotCount();
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < instanceCount; ++meshInstanceIndex) {
		for (u16 i = 0; i < materialSlotCount; ++i) {
			meshInstances[meshInstanceIndex].setMaterial(i, _materials[i]);
		}
	}

	return meshInstances;
}

const Mesh* MeshScene::findMesh(u64 assetPathHash) const {
	for (u32 i = 0; i < MESH_CAPACITY; ++i) {
		if (_assetPathHashes[i] == assetPathHash) {
			return &_meshes[i];
		}
	}

	return nullptr;
}

MeshScene* MeshScene::Get() {
	return &g_meshScene;
}
}
