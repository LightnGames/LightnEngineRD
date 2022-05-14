#include "Mesh.h"
#include <Core/Memory.h>
#include <Core/Utility.h>

namespace ltn {
namespace {
MeshScene g_meshScene;
}
void MeshScene::initialize() {
	MeshContainer::InitializetionDesc desc = {};
	desc._meshCount = 1024;
	desc._lodMeshCount = 1024 * 2;
	desc._subMeshCount = 1024 * 4;
	_meshContainer.initialize(desc);
}

void MeshScene::terminate() {
	_meshContainer.terminate();
}

Mesh* MeshScene::createMesh(const MeshCreatationDesc& desc) {
	AssetPath meshAsset(desc._assetPath);
	meshAsset.openFile();

	// 以下はメッシュエクスポーターと同一の定義にする
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
	// =========================================

	//u32 totalLodMeshCount = 0;
	//u32 totalSubMeshCount = 0;
	//u32 totalMeshletCount = 0;
	//u32 totalVertexCount = 0;
	//u32 totalVertexIndexCount = 0;
	//u32 totalPrimitiveCount = 0;
	//u32 materialSlotCount = 0;
	//u32 classicIndexCount = 0;
	//Vector3 boundsMin;
	//Vector3 boundsMax;
	//SubMeshInfoE inputSubMeshInfos[SUBMESH_PER_MESH_COUNT_MAX] = {};
	//LodInfo lodInfos[LOD_PER_MESH_COUNT_MAX] = {};
	//u64 materialNameHashes[MATERIAL_SLOT_COUNT_MAX] = {};

	//// load header
	//{
	//	fread_s(&materialSlotCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&totalSubMeshCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&totalLodMeshCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&totalMeshletCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&totalVertexCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&totalVertexIndexCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&totalPrimitiveCount, sizeof(u32), sizeof(u32), 1, fin);
	//	fread_s(&boundsMin, sizeof(Vector3), sizeof(Vector3), 1, fin);
	//	fread_s(&boundsMax, sizeof(Vector3), sizeof(Vector3), 1, fin);
	//	fread_s(&classicIndexCount, sizeof(u32), sizeof(u32), 1, fin);
	//}

	meshAsset.closeFile();
	return nullptr;
}

void MeshScene::destroyMesh(Mesh* mesh) {
}
MeshScene* MeshScene::Get() {
	return &g_meshScene;
}
void MeshContainer::initialize(const InitializetionDesc& desc) {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = desc._meshCount;
		_meshAllocationInfo.initialize(handleDesc);

		handleDesc._size = desc._lodMeshCount;
		_lodMeshAllocationInfo.initialize(handleDesc);

		handleDesc._size = desc._subMeshCount;
		_subMeshAllocationInfo.initialize(handleDesc);
	}

	{
		_meshes = Memory::allocObjects<Mesh>(desc._meshCount);
		_lodMeshes = Memory::allocObjects<LodMesh>(desc._lodMeshCount);
		_subMeshes = Memory::allocObjects<SubMesh>(desc._subMeshCount);
	}
}
void MeshContainer::terminate() {
	_meshAllocationInfo.terminate();
	_lodMeshAllocationInfo.terminate();
	_subMeshAllocationInfo.terminate();

	Memory::freeObjects(_meshes);
	Memory::freeObjects(_lodMeshes);
	Memory::freeObjects(_subMeshes);
}
Mesh* MeshContainer::allocateMesh(const MeshAllocationDesc& desc) {
	VirtualArray::AllocationInfo meshAllocationInfo = _meshAllocationInfo.allocation(1);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshAllocationInfo.allocation(desc._lodMeshCount);
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshAllocationInfo.allocation(desc._subMeshCount);

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
void MeshContainer::freeMeshObjects(Mesh* mesh) {
	_meshAllocationInfo.freeAllocation(mesh->_meshAllocationInfo);
	_lodMeshAllocationInfo.freeAllocation(mesh->_lodMeshAllocationInfo);
	_subMeshAllocationInfo.freeAllocation(mesh->_subMeshAllocationInfo);
	*mesh = Mesh();
}
}
