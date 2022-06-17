#pragma once
#include <Core/VirturalArray.h>
#include <Core/ModuleSettings.h>
#include <Core/Utility.h>
#include <Core/Math.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"

namespace ltn {
class SubMesh {
public:
	u32 _materialSlotIndex = 0;
	u32 _indexOffset = 0;
	u32 _indexCount = 0;
};

class LodMesh {
public:
	u32 _subMeshOffset = 0;
	u32 _subMeshCount = 0;
};

class Mesh {
public:
	static constexpr u16 MATERIAL_SLOT_COUNT_MAX = 16;
	static constexpr u16 INVALID_MATERIAL_SLOT_INDEX = 0xffff;

	void setLodMeshes(LodMesh* lodMeshes) { _lodMeshes = lodMeshes; }
	void setSubMeshes(SubMesh* subMeshes) { _subMeshes = subMeshes; }
	void setLodMeshCount(u32 lodMeshCount) { _lodMeshCount = lodMeshCount; }
	void setSubMeshCount(u32 subMeshCount) { _subMeshCount = subMeshCount; }
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
	const LodMesh* getLodMesh(u32 index = 0) const { return &_lodMeshes[index]; }
	const SubMesh* getSubMesh(u32 index = 0) const { return &_subMeshes[index]; }
	char* getAssetPath() { return *_assetPath; }
	u64* getMaterialSlotNameHashes() { return _materialSlotNameHashes; }
	LodMesh* getLodMesh(u32 index = 0) { return &_lodMeshes[index]; }
	SubMesh* getSubMesh(u32 index = 0) { return &_subMeshes[index]; }

private:
	LodMesh* _lodMeshes = nullptr;
	SubMesh* _subMeshes = nullptr;
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
class LTN_API MeshPool {
public:
	struct InitializetionDesc {
		u32 _meshCount = 0;
		u32 _lodMeshCount = 0;
		u32 _subMeshCount = 0;
	};

	struct MeshAllocationDesc {
		u32 _lodMeshCount = 0;
		u32 _subMeshCount = 0;
	};

	void initialize(const InitializetionDesc& desc);
	void terminate();

	Mesh* allocateMesh(const MeshAllocationDesc& desc);
	void freeMesh(const Mesh* mesh);

	u32 getMeshIndex(const Mesh* mesh) const { return static_cast<u32>(mesh - _meshes); }
	u32 getLodMeshIndex(const LodMesh* lodMesh) const { return static_cast<u32>(lodMesh - _lodMeshes); }
	u32 getSubMeshIndex(const SubMesh* subMesh) const { return static_cast<u32>(subMesh - _subMeshes); }
	Mesh* getMesh(u32 index) { return &_meshes[index]; }
	const Mesh* findMesh(u64 assetPathHash) const;

private:
	VirtualArray _meshAllocations;
	VirtualArray _lodMeshAllocations;
	VirtualArray _subMeshAllocations;
	VirtualArray::AllocationInfo* _meshAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _lodMeshAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _subMeshAllocationInfos = nullptr;

	Mesh* _meshes = nullptr;
	LodMesh* _lodMeshes = nullptr;
	SubMesh* _subMeshes = nullptr;
	u64* _meshAssetPathHashes = nullptr;
	char** _meshAssetPaths = nullptr;
	ChunkAllocator _chunkAllocator;
};

// メッシュ総合管理するクラス
class LTN_API MeshScene {
public:
	static constexpr u32 MESH_CAPACITY = 1024;
	static constexpr u32 LOD_MESH_CAPACITY = 1024 * 2;
	static constexpr u32 SUB_MESH_CAPACITY = 1024 * 4;

	struct CreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	const Mesh* createMesh(const CreatationDesc& desc);
	void destroyMesh(const Mesh* mesh);

	const MeshPool* getMeshPool() const { return &_meshPool; }
	const UpdateInfos<Mesh>* getCreateInfos() { return &_meshCreateInfos; }
	const UpdateInfos<Mesh>* getDestroyInfos() { return &_meshDestroyInfos; }
	const Mesh* findMesh(u64 assetPathHash) const { return _meshPool.findMesh(assetPathHash); }

	static MeshScene* Get();
private:
	MeshPool _meshPool;
	UpdateInfos<Mesh> _meshCreateInfos;
	UpdateInfos<Mesh> _meshDestroyInfos;
};
}