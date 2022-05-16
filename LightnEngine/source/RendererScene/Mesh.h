#pragma once
#include <Core/VirturalArray.h>
#include <Core/ModuleSettings.h>
#include <Core/Utility.h>
#include <Core/Math.h>

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
};

// メッシュインスタンスの実データを管理するクラス
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
	void freeMeshObjects(Mesh* mesh);

	Mesh* getMesh(u32 index) { return &_meshes[index]; }

private:
	VirtualArray _meshAllocations;
	VirtualArray _lodMeshAllocations;
	VirtualArray _subMeshAllocations;
	Mesh* _meshes = nullptr;
	LodMesh* _lodMeshes = nullptr;
	SubMesh* _subMeshes = nullptr;
};

// メッシュの更新情報を管理するクラス
class LTN_API MeshUpdateInfos {
public:
	Mesh** getCreatedMeshes() { return _createdMeshes; }
	Mesh** getDeletedMeshes() { return _deletedMeshes; }
	u32 getCreatedMeshCount() const { return _createdMeshCount; }
	u32 getDeletedMeshCount() const { return _deletedMeshCount; }

	void pushCreatedMesh(Mesh* mesh){
		LTN_ASSERT(_createdMeshCount < STACK_COUNT_MAX);
		_createdMeshes[_createdMeshCount++] = mesh;
	}

	void pushDeletedMesh(Mesh* mesh) {
		LTN_ASSERT(_deletedMeshCount < STACK_COUNT_MAX);
		_deletedMeshes[_deletedMeshCount++] = mesh;
	}

	void reset(){
		_createdMeshCount = 0;
		_deletedMeshCount = 0;
	}

private:
	static constexpr u32 STACK_COUNT_MAX = 128;
	Mesh* _createdMeshes[STACK_COUNT_MAX];
	Mesh* _deletedMeshes[STACK_COUNT_MAX];
	u32 _createdMeshCount = 0;
	u32 _deletedMeshCount = 0;
};

// メッシュ総合管理するクラス
class LTN_API MeshScene {
public:
	static constexpr u32 MESH_COUNT_MAX = 1024;
	static constexpr u32 LOD_MESH_COUNT_MAX = 1024 * 2;
	static constexpr u32 SUB_MESH_COUNT_MAX = 1024 * 4;

	struct MeshCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();

	void lateUpdate();

	Mesh* createMesh(const MeshCreatationDesc& desc);
	void destroyMesh(Mesh* mesh);

	MeshUpdateInfos* getMeshUpdateInfos() { return &_meshUpdateInfos; }

	static MeshScene* Get();
private:
	MeshPool _meshPool;
	MeshUpdateInfos _meshUpdateInfos;
};
}