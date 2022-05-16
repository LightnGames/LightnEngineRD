#include "Mesh.h"
#include <Core/Memory.h>
#include <Core/Utility.h>

namespace ltn {
namespace {
MeshScene g_meshScene;
}
void MeshScene::initialize() {
	MeshPool::InitializetionDesc desc = {};
	desc._meshCount = MESH_COUNT_MAX;
	desc._lodMeshCount = LOD_MESH_COUNT_MAX;
	desc._subMeshCount = SUB_MESH_COUNT_MAX;
	_meshPool.initialize(desc);
}

void MeshScene::terminate() {
	_meshPool.terminate();
}

void MeshScene::lateUpdate() {
	u32 deletedMeshCount = _meshUpdateInfos.getDeletedMeshCount();
	Mesh** deletedMeshes = _meshUpdateInfos.getDeletedMeshes();
	for (u32 i = 0; i < deletedMeshCount; ++i) {
		_meshPool.freeMeshObjects(deletedMeshes[i]);
#define ENABLE_ZERO_CLEAR 1
#if ENABLE_ZERO_CLEAR
		Mesh* mesh = deletedMeshes[i];
		memset(mesh->_lodMeshes, 0, sizeof(LodMesh) * mesh->_lodMeshCount);
		memset(mesh->_subMeshes, 0, sizeof(SubMesh) * mesh->_subMeshCount);
		memset(mesh, 0, sizeof(Mesh));
#endif
	}
	_meshUpdateInfos.reset();
}

Mesh* MeshScene::createMesh(const MeshCreatationDesc& desc) {
	AssetPath meshAsset(desc._assetPath);
	meshAsset.openFile();

	// 以下はメッシュエクスポーターと同一の定義にする
	constexpr u32 LOD_PER_MESH_COUNT_MAX = 8;
	constexpr u32 SUBMESH_PER_MESH_COUNT_MAX = 64;
	constexpr u32 MATERIAL_SLOT_COUNT_MAX = 16;

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
		u32 totalMeshletCount = 0;
		u32 totalVertexCount = 0;
		u32 totalVertexIndexCount = 0;
		u32 totalPrimitiveCount = 0;
		Float3 boundsMin;
		Float3 boundsMax;
		u32 classicIndexCount = 0;
	};
	// =========================================

	MeshHeader meshHeader;
	SubMeshInfo subMeshInfos[SUBMESH_PER_MESH_COUNT_MAX] = {};
	LodInfo lodInfos[LOD_PER_MESH_COUNT_MAX] = {};
	u64 materialNameHashes[MATERIAL_SLOT_COUNT_MAX] = {};

	meshAsset.readFile(&meshHeader, sizeof(MeshHeader));
	meshAsset.readFile(materialNameHashes, sizeof(u64) * meshHeader.materialSlotCount);
	meshAsset.readFile(subMeshInfos, sizeof(SubMeshInfo) * meshHeader.totalSubMeshCount);
	meshAsset.readFile(lodInfos, sizeof(LodInfo) * meshHeader.totalLodMeshCount);

	MeshPool::MeshAllocationDesc allocationDesc;
	allocationDesc._lodMeshCount = meshHeader.totalLodMeshCount;
	allocationDesc._subMeshCount = meshHeader.totalSubMeshCount;
	Mesh* mesh = _meshPool.allocateMesh(allocationDesc);

	mesh->_lodMeshCount = meshHeader.totalLodMeshCount;
	mesh->_subMeshCount = meshHeader.totalSubMeshCount;
	mesh->_vertexCount = meshHeader.totalVertexCount;
	mesh->_indexCount = meshHeader.classicIndexCount;

	LodMesh* lodMeshes = mesh->_lodMeshes;
	for (u32 i = 0; i < meshHeader.totalLodMeshCount; ++i) {
		LodInfo& lodInfo = lodInfos[i];
		LodMesh& lodMesh = lodMeshes[i];
		lodMesh._subMeshCount = lodInfo._subMeshCount;
		lodMesh._subMeshOffset = lodInfo._subMeshOffset;
	}

	SubMesh* subMeshes = mesh->_subMeshes;
	for (u32 i = 0; i < meshHeader.totalSubMeshCount; ++i) {
		SubMeshInfo& subMeshInfo = subMeshInfos[i];
		SubMesh& subMesh = subMeshes[i];
		subMesh._materialSlotIndex = subMeshInfo._materialSlotIndex;
		subMesh._indexCount = subMeshInfo._triangleStripIndexCount;
		subMesh._indexOffset = subMeshInfo._triangleStripIndexOffset;
	}

	_meshUpdateInfos.pushCreatedMesh(mesh);

	meshAsset.closeFile();
	return mesh;
}

void MeshScene::destroyMesh(Mesh* mesh) {
	_meshUpdateInfos.pushDeletedMesh(mesh);
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
	}
}
void MeshPool::terminate() {
	_meshAllocations.terminate();
	_lodMeshAllocations.terminate();
	_subMeshAllocations.terminate();

	Memory::freeObjects(_meshes);
	Memory::freeObjects(_lodMeshes);
	Memory::freeObjects(_subMeshes);
}
Mesh* MeshPool::allocateMesh(const MeshAllocationDesc& desc) {
	VirtualArray::AllocationInfo meshAllocationInfo = _meshAllocations.allocation(1);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshAllocations.allocation(desc._lodMeshCount);
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshAllocations.allocation(desc._subMeshCount);

	Mesh* mesh = &_meshes[meshAllocationInfo._offset];
	mesh->_meshAllocationInfo = meshAllocationInfo;
	mesh->_lodMeshAllocationInfo = lodMeshAllocationInfo;
	mesh->_subMeshAllocationInfo = subMeshAllocationInfo;
	mesh->_lodMeshes = &_lodMeshes[lodMeshAllocationInfo._offset];
	mesh->_subMeshes = &_subMeshes[subMeshAllocationInfo._offset];
	mesh->_lodMeshCount = desc._lodMeshCount;
	mesh->_subMeshCount = desc._subMeshCount;
	return mesh;
}
void MeshPool::freeMeshObjects(Mesh* mesh) {
	_meshAllocations.freeAllocation(mesh->_meshAllocationInfo);
	_lodMeshAllocations.freeAllocation(mesh->_lodMeshAllocationInfo);
	_subMeshAllocations.freeAllocation(mesh->_subMeshAllocationInfo);
}
}
