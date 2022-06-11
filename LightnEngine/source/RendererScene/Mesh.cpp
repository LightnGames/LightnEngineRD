#include "Mesh.h"
#include <Core/Memory.h>
#include <Core/Utility.h>

namespace ltn {
namespace {
MeshScene g_meshScene;
}
void MeshScene::initialize() {
	MeshPool::InitializetionDesc desc = {};
	desc._meshCount = MESH_CAPACITY;
	desc._lodMeshCount = LOD_MESH_CAPACITY;
	desc._subMeshCount = SUB_MESH_CAPACITY;
	_meshPool.initialize(desc);
}

void MeshScene::terminate() {
	lateUpdate();
	_meshPool.terminate();
}

void MeshScene::lateUpdate() {
	u32 destroyMeshCount = _meshDestroyInfos.getUpdateCount();
	auto destroyMeshes = _meshDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		_meshPool.freeMesh(destroyMeshes[i]);
#define ENABLE_ZERO_CLEAR 1
#if ENABLE_ZERO_CLEAR
		Mesh* mesh = _meshPool.getMesh(_meshPool.getMeshIndex(destroyMeshes[i]));
		memset(mesh->getLodMesh(), 0, sizeof(LodMesh) * mesh->getLodMeshCount());
		memset(mesh->getSubMesh(), 0, sizeof(SubMesh) * mesh->getSubMeshCount());
		memset(mesh, 0, sizeof(Mesh));
#endif
	}
	_meshCreateInfos.reset();
	_meshDestroyInfos.reset();
}

Mesh* MeshScene::createMesh(const CreatationDesc& desc) {
	AssetPath meshAsset(desc._assetPath);
	meshAsset.openFile();

	// 以下はメッシュエクスポーターと同一の定義にする
	constexpr u32 LOD_PER_MESH_COUNT_MAX = 8;
	constexpr u32 SUBMESH_PER_MESH_COUNT_MAX = 64;

	struct SubMeshInfo {
		u32 _materialSlotIndex = 0;
		u32 _meshletCount = 0;
		u32 _meshletStartIndex = 0;
		u32 _triangleStripIndexCount = 0;
		u32 _triangleStripIndexOffset = 0;
	};

	struct LodInfo {
		u32 _vertexOffset = 0;
		u32 _vertexIndexOffset = 0;
		u32 _primitiveOffset = 0;
		u32 _indexOffset = 0;
		u32 _subMeshOffset = 0;
		u32 _subMeshCount = 0;
		u32 _vertexCount = 0;
		u32 _vertexIndexCount = 0;
		u32 _primitiveCount = 0;
	};

	struct MeshHeader {
		u32 materialSlotCount = 0;
		u32 totalSubMeshCount = 0;
		u32 totalLodMeshCount = 0;
		u32 totalVertexCount = 0;
		u32 totalIndexCount = 0;
		Float3 boundsMin;
		Float3 boundsMax;
	};
	// =========================================

	MeshHeader meshHeader;
	SubMeshInfo subMeshInfos[SUBMESH_PER_MESH_COUNT_MAX] = {};
	LodInfo lodInfos[LOD_PER_MESH_COUNT_MAX] = {};

	meshAsset.readFile(&meshHeader, sizeof(MeshHeader));

	MeshPool::MeshAllocationDesc allocationDesc;
	allocationDesc._lodMeshCount = meshHeader.totalLodMeshCount;
	allocationDesc._subMeshCount = meshHeader.totalSubMeshCount;
	Mesh* mesh = _meshPool.allocateMesh(allocationDesc);

	meshAsset.readFile(mesh->getMaterialSlotNameHashes(), sizeof(u64) * meshHeader.materialSlotCount);
	meshAsset.readFile(subMeshInfos, sizeof(SubMeshInfo) * meshHeader.totalSubMeshCount);
	meshAsset.readFile(lodInfos, sizeof(LodInfo) * meshHeader.totalLodMeshCount);

	mesh->setLodMeshCount(meshHeader.totalLodMeshCount);
	mesh->setSubMeshCount(meshHeader.totalSubMeshCount);
	mesh->setVertexCount(meshHeader.totalVertexCount);
	mesh->setIndexCount(meshHeader.totalIndexCount);
	mesh->setMaterialSlotCount(meshHeader.materialSlotCount);
	mesh->setAssetPathHash(StrHash64(desc._assetPath));

	u32 assetPathLength = StrLength(desc._assetPath) + 1;
	mesh->setAssetPath(Memory::allocObjects<char>(assetPathLength));
	memcpy(mesh->getAssetPath(), desc._assetPath, assetPathLength);

	LodMesh* lodMeshes = mesh->getLodMesh();
	for (u32 i = 0; i < meshHeader.totalLodMeshCount; ++i) {
		const LodInfo& lodInfo = lodInfos[i];
		LodMesh& lodMesh = lodMeshes[i];
		lodMesh._subMeshCount = lodInfo._subMeshCount;
		lodMesh._subMeshOffset = lodInfo._subMeshOffset;
	}

	SubMesh* subMeshes = mesh->getSubMesh();
	for (u32 i = 0; i < meshHeader.totalSubMeshCount; ++i) {
		const SubMeshInfo& subMeshInfo = subMeshInfos[i];
		SubMesh& subMesh = subMeshes[i];
		subMesh._materialSlotIndex = subMeshInfo._materialSlotIndex;
		subMesh._indexCount = subMeshInfo._triangleStripIndexCount;
		subMesh._indexOffset = subMeshInfo._triangleStripIndexOffset;
	}

	_meshCreateInfos.push(mesh);

	meshAsset.closeFile();
	return mesh;
}

void MeshScene::destroyMesh(Mesh* mesh) {
	_meshDestroyInfos.push(mesh);
}

MeshScene* MeshScene::Get() {
	return &g_meshScene;
}

void MeshPool::initialize(const InitializetionDesc& desc) {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = desc._meshCount;
		_meshAllocations.initialize(handleDesc);

		handleDesc._size = desc._lodMeshCount;
		_lodMeshAllocations.initialize(handleDesc);

		handleDesc._size = desc._subMeshCount;
		_subMeshAllocations.initialize(handleDesc);
	}

	{
		_meshes = Memory::allocObjects<Mesh>(desc._meshCount);
		_lodMeshes = Memory::allocObjects<LodMesh>(desc._lodMeshCount);
		_subMeshes = Memory::allocObjects<SubMesh>(desc._subMeshCount);
		_meshAssetPathHashes = Memory::allocObjects<u64>(desc._meshCount);
		_meshAssetPaths = Memory::allocObjects<char*>(desc._meshCount);

		_meshAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(desc._meshCount);
		_lodMeshAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(desc._lodMeshCount);
		_subMeshAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(desc._subMeshCount);
	}
}

void MeshPool::terminate() {
	_meshAllocations.terminate();
	_lodMeshAllocations.terminate();
	_subMeshAllocations.terminate();

	Memory::freeObjects(_meshes);
	Memory::freeObjects(_lodMeshes);
	Memory::freeObjects(_subMeshes);
	Memory::freeObjects(_meshAssetPathHashes);
	Memory::freeObjects(_meshAssetPaths);
	Memory::freeObjects(_meshAllocationInfos);
	Memory::freeObjects(_lodMeshAllocationInfos);
	Memory::freeObjects(_subMeshAllocationInfos);
}

Mesh* MeshPool::allocateMesh(const MeshAllocationDesc& desc) {
	VirtualArray::AllocationInfo meshAllocationInfo = _meshAllocations.allocation(1);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshAllocations.allocation(desc._lodMeshCount);
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshAllocations.allocation(desc._subMeshCount);

	_meshAllocationInfos[meshAllocationInfo._offset] = meshAllocationInfo;
	_lodMeshAllocationInfos[meshAllocationInfo._offset] = lodMeshAllocationInfo;
	_subMeshAllocationInfos[meshAllocationInfo._offset] = subMeshAllocationInfo;

	Mesh* mesh = &_meshes[meshAllocationInfo._offset];
	mesh->setLodMeshes(&_lodMeshes[lodMeshAllocationInfo._offset]);
	mesh->setSubMeshes(&_subMeshes[subMeshAllocationInfo._offset]);
	mesh->setAssetPathHashPtr(&_meshAssetPathHashes[meshAllocationInfo._offset]);
	mesh->setAssetPathPtr(&_meshAssetPaths[meshAllocationInfo._offset]);
	return mesh;
}

void MeshPool::freeMesh(const Mesh* mesh) {
	u32 meshIndex = getMeshIndex(mesh);
	_meshAllocations.freeAllocation(_meshAllocationInfos[meshIndex]);
	_lodMeshAllocations.freeAllocation(_lodMeshAllocationInfos[meshIndex]);
	_subMeshAllocations.freeAllocation(_subMeshAllocationInfos[meshIndex]);
	Memory::freeObjects(_meshAssetPaths[getMeshIndex(mesh)]);
}
Mesh* MeshPool::findMesh(u64 assetPathHash) {
	for (u32 i = 0; i < MeshScene::MESH_CAPACITY; ++i) {
		if (_meshAssetPathHashes[i] == assetPathHash) {
			return &_meshes[i];
		}
	}
	return nullptr;
}
u16 Mesh::findMaterialSlotIndex(u64 slotNameHash) const {
	for (u32 i = 0; i < _materialSlotCount; ++i) {
		if (_materialSlotNameHashes[i] == slotNameHash) {
			return i;
		}
	}

	return INVALID_MATERIAL_SLOT_INDEX;
}
}
