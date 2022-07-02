#include "MeshGeometry.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <Core/CpuTimerManager.h>

namespace ltn {
namespace {
MeshGeometryScene g_MeshGeometryScene;
}
void MeshGeometryScene::initialize() {
	CpuScopedPerf scopedPerf("MeshGeometryScene");
	MeshGeometryPool::InitializetionDesc desc = {};
	desc._meshCount = MESH_GEOMETRY_CAPACITY;
	desc._lodMeshCount = LOD_MESH_GEOMETRY_CAPACITY;
	desc._subMeshCount = SUB_MESH_GEOMETRY_CAPACITY;
	_meshGeometryPool.initialize(desc);
}

void MeshGeometryScene::terminate() {
	lateUpdate();
	_meshGeometryPool.terminate();
}

void MeshGeometryScene::lateUpdate() {
	u32 destroyMeshCount = _destroyInfos.getUpdateCount();
	auto destroyMeshes = _destroyInfos.getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		_meshGeometryPool.freeMeshGeometry(destroyMeshes[i]);
#define ENABLE_ZERO_CLEAR 1
#if ENABLE_ZERO_CLEAR
		MeshGeometry* mesh = _meshGeometryPool.getMeshGeometry(_meshGeometryPool.getMeshGeometryIndex(destroyMeshes[i]));
		memset(mesh->getLodMesh(), 0, sizeof(LodMeshGeometry) * mesh->getLodMeshCount());
		memset(mesh->getSubMesh(), 0, sizeof(SubMeshGeometry) * mesh->getSubMeshCount());
		memset(mesh, 0, sizeof(MeshGeometry));
#endif
	}
	_createInfos.reset();
	_destroyInfos.reset();
}

const MeshGeometry* MeshGeometryScene::createMeshGeometry(const CreatationDesc& desc) {
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
		u32 _vertexCount = 0;
		u32 _indexOffset = 0;
		u32 _indexCount;
		u32 _subMeshOffset = 0;
		u32 _subMeshCount = 0;
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

	MeshGeometryPool::AllocationDesc allocationDesc;
	allocationDesc._lodMeshCount = meshHeader.totalLodMeshCount;
	allocationDesc._subMeshCount = meshHeader.totalSubMeshCount;
	MeshGeometry* mesh = _meshGeometryPool.allocateMeshGeometry(allocationDesc);

	meshAsset.readFile(mesh->getMaterialSlotNameHashes(), sizeof(u64) * meshHeader.materialSlotCount);
	meshAsset.readFile(subMeshInfos, sizeof(SubMeshInfo) * meshHeader.totalSubMeshCount);
	meshAsset.readFile(lodInfos, sizeof(LodInfo) * meshHeader.totalLodMeshCount);

	mesh->setLodMeshGeometryCount(meshHeader.totalLodMeshCount);
	mesh->setSubMeshGeometryCount(meshHeader.totalSubMeshCount);
	mesh->setVertexCount(meshHeader.totalVertexCount);
	mesh->setIndexCount(meshHeader.totalIndexCount);
	mesh->setMaterialSlotCount(meshHeader.materialSlotCount);
	mesh->setAssetPathHash(StrHash64(desc._assetPath));
	mesh->setBoundsMin(Vector3(meshHeader.boundsMin));
	mesh->setBoundsMax(Vector3(meshHeader.boundsMax));

	u32 assetPathLength = StrLength(desc._assetPath) + 1;
	mesh->setAssetPath(Memory::allocObjects<char>(assetPathLength));
	memcpy(mesh->getAssetPath(), desc._assetPath, assetPathLength);

	LodMeshGeometry* lodMeshes = mesh->getLodMesh();
	for (u32 i = 0; i < meshHeader.totalLodMeshCount; ++i) {
		const LodInfo& lodInfo = lodInfos[i];
		LodMeshGeometry& lodMesh = lodMeshes[i];
		lodMesh._subMeshCount = lodInfo._subMeshCount;
		lodMesh._subMeshOffset = lodInfo._subMeshOffset;
		lodMesh._vertexCount = lodInfo._vertexCount;
		lodMesh._indexOffset = lodInfo._indexOffset;
		lodMesh._indexCount = lodInfo._indexCount;
		lodMesh._vertexOffset = lodInfo._vertexOffset;
	}

	SubMeshGeometry* subMeshes = mesh->getSubMesh();
	for (u32 i = 0; i < meshHeader.totalSubMeshCount; ++i) {
		const SubMeshInfo& subMeshInfo = subMeshInfos[i];
		SubMeshGeometry& subMesh = subMeshes[i];
		subMesh._materialSlotIndex = subMeshInfo._materialSlotIndex;
		subMesh._indexCount = subMeshInfo._triangleStripIndexCount;
		subMesh._indexOffset = subMeshInfo._triangleStripIndexOffset;
	}

	_createInfos.push(mesh);

	meshAsset.closeFile();
	return mesh;
}

void MeshGeometryScene::createMeshGeometries(const CreatationDesc* descs, MeshGeometry const** meshGeometries, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		const CreatationDesc* desc = descs + i;
		meshGeometries[i] = createMeshGeometry(*desc);
	}
}

void MeshGeometryScene::destroyMeshGeometry(const MeshGeometry* mesh) {
	_destroyInfos.push(mesh);
}

void MeshGeometryScene::destroyMeshGeometries(MeshGeometry const** meshes, u32 instanceCount) {
	for (u32 i = 0; i < instanceCount; ++i) {
		destroyMeshGeometry(meshes[i]);
	}
}

MeshGeometryScene* MeshGeometryScene::Get() {
	return &g_MeshGeometryScene;
}

void MeshGeometryPool::initialize(const InitializetionDesc& desc) {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = desc._meshCount;
		_meshAllocations.initialize(handleDesc);

		handleDesc._size = desc._lodMeshCount;
		_lodMeshAllocations.initialize(handleDesc);

		handleDesc._size = desc._subMeshCount;
		_subMeshAllocations.initialize(handleDesc);
	}

	_chunkAllocator.allocate([this, desc](ChunkAllocator::Allocation& allocation) {
		_meshGeometries = allocation.allocateObjects<MeshGeometry>(desc._meshCount);
		_lodMeshGeometries = allocation.allocateObjects<LodMeshGeometry>(desc._lodMeshCount);
		_subMeshGeometries = allocation.allocateObjects<SubMeshGeometry>(desc._subMeshCount);
		_assetPathHashes = allocation.allocateObjects<u64>(desc._meshCount);
		_assetPaths = allocation.allocateObjects<char*>(desc._meshCount);
		_meshAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(desc._meshCount);
		_lodMeshAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(desc._lodMeshCount);
		_subMeshAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(desc._subMeshCount);
	});
}

void MeshGeometryPool::terminate() {
	_meshAllocations.terminate();
	_lodMeshAllocations.terminate();
	_subMeshAllocations.terminate();
	_chunkAllocator.free();
}

MeshGeometry* MeshGeometryPool::allocateMeshGeometry(const AllocationDesc& desc) {
	VirtualArray::AllocationInfo meshAllocationInfo = _meshAllocations.allocation(1);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshAllocations.allocation(desc._lodMeshCount);
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshAllocations.allocation(desc._subMeshCount);

	_meshAllocationInfos[meshAllocationInfo._offset] = meshAllocationInfo;
	_lodMeshAllocationInfos[meshAllocationInfo._offset] = lodMeshAllocationInfo;
	_subMeshAllocationInfos[meshAllocationInfo._offset] = subMeshAllocationInfo;

	MeshGeometry* mesh = &_meshGeometries[meshAllocationInfo._offset];
	mesh->setLodMeshGeometries(&_lodMeshGeometries[lodMeshAllocationInfo._offset]);
	mesh->setSubMeshGeometries(&_subMeshGeometries[subMeshAllocationInfo._offset]);
	mesh->setAssetPathHashPtr(&_assetPathHashes[meshAllocationInfo._offset]);
	mesh->setAssetPathPtr(&_assetPaths[meshAllocationInfo._offset]);
	return mesh;
}

void MeshGeometryPool::freeMeshGeometry(const MeshGeometry* mesh) {
	u32 meshIndex = getMeshGeometryIndex(mesh);
	_meshAllocations.freeAllocation(_meshAllocationInfos[meshIndex]);
	_lodMeshAllocations.freeAllocation(_lodMeshAllocationInfos[meshIndex]);
	_subMeshAllocations.freeAllocation(_subMeshAllocationInfos[meshIndex]);
	Memory::freeObjects(_assetPaths[getMeshGeometryIndex(mesh)]);
}

const MeshGeometry* MeshGeometryPool::findMeshGeometry(u64 assetPathHash) const {
	for (u32 i = 0; i < MeshGeometryScene::MESH_GEOMETRY_CAPACITY; ++i) {
		if (_assetPathHashes[i] == assetPathHash) {
			return &_meshGeometries[i];
		}
	}
	return nullptr;
}

u16 MeshGeometry::findMaterialSlotIndex(u64 slotNameHash) const {
	for (u32 i = 0; i < _materialSlotCount; ++i) {
		if (_materialSlotNameHashes[i] == slotNameHash) {
			return i;
		}
	}

	return INVALID_MATERIAL_SLOT_INDEX;
}
}
