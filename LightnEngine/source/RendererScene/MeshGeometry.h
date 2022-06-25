#pragma once
#include <Core/VirturalArray.h>
#include <Core/ModuleSettings.h>
#include <Core/Utility.h>
#include <Core/Math.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"

namespace ltn {
class SubMeshGeometry {
public:
	u32 _materialSlotIndex = 0;
	u32 _indexOffset = 0;
	u32 _indexCount = 0;
};

class LodMeshGeometry {
public:
	u32 _vertexOffset = 0;
	u32 _vertexCount = 0;
	u32 _indexOffset = 0;
	u32 _indexCount = 0;
	u32 _subMeshOffset = 0;
	u32 _subMeshCount = 0;
};

class MeshGeometry {
public:
	static constexpr u16 MATERIAL_SLOT_COUNT_MAX = 16;
	static constexpr u16 INVALID_MATERIAL_SLOT_INDEX = 0xffff;

	void setLodMeshGeometries(LodMeshGeometry* lodMeshes) { _lodMeshGeometries = lodMeshes; }
	void setSubMeshGeometries(SubMeshGeometry* subMeshes) { _subMeshGeometries = subMeshes; }
	void setLodMeshGeometryCount(u32 lodMeshCount) { _lodMeshCount = lodMeshCount; }
	void setSubMeshGeometryCount(u32 subMeshCount) { _subMeshCount = subMeshCount; }
	void setVertexCount(u32 vertexCount) { _vertexCount = vertexCount; }
	void setIndexCount(u32 indexCount) { _indexCount = indexCount; }
	void setMaterialSlotCount(u16 materialSlotCount) { _materialSlotCount = materialSlotCount; }
	void setAssetPathHashPtr(u64* assetPathHash) { _assetPathHash = assetPathHash; }
	void setAssetPathPtr(char** assetPath) { _assetPath = assetPath; }
	void setAssetPathHash(u64 assetPathHash) { *_assetPathHash = assetPathHash; }
	void setAssetPath(char* assetPath) { *_assetPath = assetPath; }

	u16 findMaterialSlotIndex(u64 slotNameHash) const;
	u32 getLodMeshCount() const { return _lodMeshCount; }
	u32 getSubMeshCount() const { return _subMeshCount; }
	u32 getVertexCount() const { return _vertexCount; }
	u32 getIndexCount() const { return _indexCount; }
	u16 getMaterialSlotCount() const { return _materialSlotCount; }
	u64 getAssetPathHash() const { return *_assetPathHash; }

	const char* getAssetPath() const { return *_assetPath; }
	const u64* getMaterialSlotNameHashes() const { return _materialSlotNameHashes; }
	const LodMeshGeometry* getLodMesh(u32 index = 0) const { return &_lodMeshGeometries[index]; }
	const SubMeshGeometry* getSubMesh(u32 index = 0) const { return &_subMeshGeometries[index]; }
	char* getAssetPath() { return *_assetPath; }
	u64* getMaterialSlotNameHashes() { return _materialSlotNameHashes; }
	LodMeshGeometry* getLodMesh(u32 index = 0) { return &_lodMeshGeometries[index]; }
	SubMeshGeometry* getSubMesh(u32 index = 0) { return &_subMeshGeometries[index]; }

private:
	LodMeshGeometry* _lodMeshGeometries = nullptr;
	SubMeshGeometry* _subMeshGeometries = nullptr;
	u32 _lodMeshCount = 0;
	u32 _subMeshCount = 0;
	u32 _vertexCount = 0;
	u32 _indexCount = 0;
	u16 _materialSlotCount;
	u64 _materialSlotNameHashes[MATERIAL_SLOT_COUNT_MAX];
	u64* _assetPathHash = nullptr;
	char** _assetPath = nullptr;
};

// メッシュの実データを管理するプール
class MeshGeometryPool {
public:
	struct InitializetionDesc {
		u32 _meshCount = 0;
		u32 _lodMeshCount = 0;
		u32 _subMeshCount = 0;
	};

	struct AllocationDesc {
		u32 _lodMeshCount = 0;
		u32 _subMeshCount = 0;
	};

	void initialize(const InitializetionDesc& desc);
	void terminate();

	MeshGeometry* allocateMeshGeometry(const AllocationDesc& desc);
	void freeMeshGeometry(const MeshGeometry* mesh);

	u32 getMeshGeometryIndex(const MeshGeometry* mesh) const { return static_cast<u32>(mesh - _meshGeometries); }
	u32 getLodMeshGeometryIndex(const LodMeshGeometry* lodMesh) const { return static_cast<u32>(lodMesh - _lodMeshGeometries); }
	u32 getSubMeshGeometryIndex(const SubMeshGeometry* subMesh) const { return static_cast<u32>(subMesh - _subMeshGeometries); }
	MeshGeometry* getMeshGeometry(u32 index) { return &_meshGeometries[index]; }
	const MeshGeometry* findMeshGeometry(u64 assetPathHash) const;

private:
	VirtualArray _meshAllocations;
	VirtualArray _lodMeshAllocations;
	VirtualArray _subMeshAllocations;
	VirtualArray::AllocationInfo* _meshAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _lodMeshAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _subMeshAllocationInfos = nullptr;

	MeshGeometry* _meshGeometries = nullptr;
	LodMeshGeometry* _lodMeshGeometries = nullptr;
	SubMeshGeometry* _subMeshGeometries = nullptr;
	u64* _assetPathHashes = nullptr;
	char** _assetPaths = nullptr;
	ChunkAllocator _chunkAllocator;
};

// メッシュ総合管理するクラス
class MeshGeometryScene {
public:
	static constexpr u32 MESH_GEOMETRY_CAPACITY = 1024;
	static constexpr u32 LOD_MESH_GEOMETRY_CAPACITY = 1024 * 2;
	static constexpr u32 SUB_MESH_GEOMETRY_CAPACITY = 1024 * 4;

	struct CreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	const MeshGeometry* createMeshGeometry(const CreatationDesc& desc);
	void createMeshGeometries(const CreatationDesc* descs, MeshGeometry const** meshgeometries, u32 instanceCount);
	void destroyMeshGeometry(const MeshGeometry* mesh);
	void destroyMeshGeometries(MeshGeometry const** meshes, u32 instanceCount);

	const MeshGeometryPool* getMeshGeometryPool() const { return &_meshGeometryPool; }
	const UpdateInfos<MeshGeometry>* getCreateInfos() { return &_createInfos; }
	const UpdateInfos<MeshGeometry>* getDestroyInfos() { return &_destroyInfos; }
	const MeshGeometry* findMeshGeometry(u64 assetPathHash) const { return _meshGeometryPool.findMeshGeometry(assetPathHash); }

	static MeshGeometryScene* Get();
private:
	MeshGeometryPool _meshGeometryPool;
	UpdateInfos<MeshGeometry> _createInfos;
	UpdateInfos<MeshGeometry> _destroyInfos;
};
}