#pragma once
#include <Core/Type.h>
#include <Core/Utility.h>
#include <Core/VirturalArray.h>
#include "RenderSceneUtility.h"

namespace ltn {
class Mesh;
class MaterialInstance;
class SubMeshInstance {
public:
	MaterialInstance* _materialInstance = nullptr;
};

class LodMeshInstance {
public:
	f32 _lodThreshold = 0.0f;
};

class MeshInstance {
public:
	VirtualArray::AllocationInfo _meshAllocationInfo;
	VirtualArray::AllocationInfo _lodMeshAllocationInfo;
	VirtualArray::AllocationInfo _subMeshAllocationInfo;

	SubMeshInstance* _subMeshInstances = nullptr;
	LodMeshInstance* _lodMeshInstances = nullptr;
	const Mesh* _mesh = nullptr;
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
	VirtualArray _meshAllocations;
	VirtualArray _lodMeshAllocations;
	VirtualArray _subMeshAllocations;
	MeshInstance* _meshInstances = nullptr;
	LodMeshInstance* _lodMeshInstances = nullptr;
	SubMeshInstance* _subMeshInstances = nullptr;
};

class MeshInstanceScene {
public:
	static constexpr u32 MESH_INSTANCE_CAPACITY = 1024;
	static constexpr u32 LOD_MESH_INSTANCE_CAPACITY = 1024 * 4;
	static constexpr u32 SUB_MESH_INSTANCE_CAPACITY = 1024 * 8;

	struct MeshInstanceCreatationDesc {
		const Mesh* _mesh = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	MeshInstance* createMeshInstance(const MeshInstanceCreatationDesc& desc);
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