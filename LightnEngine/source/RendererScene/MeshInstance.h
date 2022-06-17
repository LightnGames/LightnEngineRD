#pragma once
#include <Core/Type.h>
#include <Core/Utility.h>
#include <Core/VirturalArray.h>
#include <Core/Math.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"

namespace ltn {
class Mesh;
class Material;
class SubMeshInstance {
public:
	void setMaterial(const Material* material) { _material = material; }
	const Material* getMaterial() const { return _material; }

private:
	const Material* _material = nullptr;
};

class LodMeshInstance {
public:
	void setLodThreshold(f32 threshold) { _lodThreshold = threshold; }
	f32 getLodThreshold() const { return _lodThreshold; }

private:
	f32 _lodThreshold = 0.0f;
};

class MeshInstance {
public:
	void setMesh(const Mesh* mesh) { _mesh = mesh; }
	void setLodMeshInstances(LodMeshInstance* lodMeshInstances) { _lodMeshInstances = lodMeshInstances; }
	void setSubMeshInstances(SubMeshInstance* subMeshInstances) { _subMeshInstances = subMeshInstances; }

	void setMaterial(u16 materialSlotIndex, const Material* material);
	void setMaterial(u64 materialSlotNameHash, const Material* material);
	void setWorldMatrix(const Matrix4& worldMatrix, bool dirtyFlags = true);

	const Mesh* getMesh() const { return _mesh; }
	const LodMeshInstance* getLodMeshInstance(u32 index = 0) const { return &_lodMeshInstances[index]; }
	const SubMeshInstance* getSubMeshInstance(u32 index = 0) const { return &_subMeshInstances[index]; }
	LodMeshInstance* getLodMeshInstance(u32 index = 0) { return &_lodMeshInstances[index]; }
	SubMeshInstance* getSubMeshInstance(u32 index = 0) { return &_subMeshInstances[index]; }
	Matrix4 getWorldMatrix() const { return _worldMatrix; }

private:
	SubMeshInstance* _subMeshInstances = nullptr;
	LodMeshInstance* _lodMeshInstances = nullptr;
	const Mesh* _mesh = nullptr;
	Matrix4 _worldMatrix;
};

// メッシュインスタンスの実データを管理するクラス
class MeshInstancePool {
public:
	struct InitializetionDesc {
		u32 _meshInstanceCount = 0;
		u32 _lodMeshInstanceCount = 0;
		u32 _subMeshInstanceCount = 0;
	};

	struct MeshAllocationDesc {
		const Mesh* _mesh = nullptr;
	};

	void initialize(const InitializetionDesc& desc);
	void terminate();

	MeshInstance* allocateMeshInstance(const MeshAllocationDesc& desc);
	void freeMeshInstance(const MeshInstance* meshInstance);

	u32 getMeshInstanceIndex(const MeshInstance* mesh) const { return static_cast<u32>(mesh - _meshInstances); }
	u32 getLodMeshInstanceIndex(const LodMeshInstance* lodMesh) const { return static_cast<u32>(lodMesh - _lodMeshInstances); }
	u32 getSubMeshInstanceIndex(const SubMeshInstance* subMesh) const { return static_cast<u32>(subMesh - _subMeshInstances); }
	MeshInstance* getMeshInstance(u32 index) { return &_meshInstances[index]; }

private:
	VirtualArray _meshInstanceAllocations;
	VirtualArray _lodMeshInstanceAllocations;
	VirtualArray _subMeshInstanceAllocations;
	
	VirtualArray::AllocationInfo* _meshInstanceAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _lodMeshInstanceAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _subMeshInstanceAllocationInfos = nullptr;
	
	MeshInstance* _meshInstances = nullptr;
	LodMeshInstance* _lodMeshInstances = nullptr;
	SubMeshInstance* _subMeshInstances = nullptr;
	ChunkAllocator _chunkAllocator;
};

class MeshInstanceScene {
public:
	static constexpr u32 MESH_INSTANCE_CAPACITY = 1024;
	static constexpr u32 LOD_MESH_INSTANCE_CAPACITY = 1024 * 4;
	static constexpr u32 SUB_MESH_INSTANCE_CAPACITY = 1024 * 8;

	struct CreatationDesc {
		const Mesh* _mesh = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	MeshInstance* createMeshInstance(const CreatationDesc& desc);
	void destroyMeshInstance(MeshInstance* meshInstance);

	const MeshInstancePool* getMeshInstancePool() const { return &_meshInstancePool; }

	UpdateInfos<MeshInstance>* getMeshInstanceCreateInfos() { return &_meshInstanceCreateInfos; }
	UpdateInfos<MeshInstance>* getMeshInstanceUpdateInfos() { return &_meshInstanceUpdateInfos; }
	UpdateInfos<MeshInstance>* getMeshInstanceDestroyInfos() { return &_meshInstanceDestroyInfos; }
	UpdateInfos<LodMeshInstance>* getLodMeshInstanceUpdateInfos() { return &_lodMeshInstanceUpdateInfos; }
	UpdateInfos<SubMeshInstance>* getSubMeshInstanceUpdateInfos() { return &_subMeshInstanceUpdateInfos; }

	static MeshInstanceScene* Get();

private:
	MeshInstancePool _meshInstancePool;
	UpdateInfos<MeshInstance> _meshInstanceCreateInfos;
	UpdateInfos<MeshInstance> _meshInstanceDestroyInfos;
	UpdateInfos<MeshInstance> _meshInstanceUpdateInfos;
	UpdateInfos<LodMeshInstance> _lodMeshInstanceUpdateInfos;
	UpdateInfos<SubMeshInstance> _subMeshInstanceUpdateInfos;
};
}