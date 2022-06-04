#pragma once
#include <Core/VirturalArray.h>
#include <Core/ModuleSettings.h>
#include <Core/Utility.h>
#include <Core/Math.h>
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
	VirtualArray::AllocationInfo _meshAllocationInfo;
	VirtualArray::AllocationInfo _lodMeshAllocationInfo;
	VirtualArray::AllocationInfo _subMeshAllocationInfo;
	SubMesh* _subMeshes = nullptr;
	LodMesh* _lodMeshes = nullptr;
	u32 _lodMeshCount = 0;
	u32 _subMeshCount = 0;
	u32 _vertexCount = 0;
	u32 _indexCount = 0;
	u64* _assetPathHash = nullptr;
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
	Mesh* findMesh(u64 assetPathHash);

private:
	VirtualArray _meshAllocations;
	VirtualArray _lodMeshAllocations;
	VirtualArray _subMeshAllocations;
	Mesh* _meshes = nullptr;
	LodMesh* _lodMeshes = nullptr;
	SubMesh* _subMeshes = nullptr;
	u64* _meshAssetPathHashes = nullptr;
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

	Mesh* createMesh(const CreatationDesc& desc);
	void destroyMesh(Mesh* mesh);

	const MeshPool* getMeshPool() const { return &_meshPool; }
	const UpdateInfos<Mesh>* geCreateInfos() { return &_meshCreateInfos; }
	const UpdateInfos<Mesh>* getDestroyInfos() { return &_meshDestroyInfos; }
	Mesh* findMesh(u64 assetPathHash) { return _meshPool.findMesh(assetPathHash); }

	static MeshScene* Get();
private:
	MeshPool _meshPool;
	UpdateInfos<Mesh> _meshCreateInfos;
	UpdateInfos<Mesh> _meshDestroyInfos;
};
}