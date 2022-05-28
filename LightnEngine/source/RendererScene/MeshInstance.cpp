#include "MeshInstance.h"
#include <Core/Memory.h>
#include <RendererScene/Mesh.h>

namespace ltn {
namespace {
MeshInstanceScene g_meshInstanceScene;
}
void MeshInstancePool::initialize(const InitializetionDesc& desc) {
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = desc._meshInstanceCount;
		_meshAllocations.initialize(handleDesc);

		handleDesc._size = desc._lodMeshInstanceCount;
		_lodMeshAllocations.initialize(handleDesc);

		handleDesc._size = desc._subMeshInstanceCount;
		_subMeshAllocations.initialize(handleDesc);
	}

	_meshInstances = Memory::allocObjects<MeshInstance>(desc._meshInstanceCount);
	_lodMeshInstances = Memory::allocObjects<LodMeshInstance>(desc._lodMeshInstanceCount);
	_subMeshInstances = Memory::allocObjects<SubMeshInstance>(desc._subMeshInstanceCount);
}

void MeshInstancePool::terminate() {
	_meshAllocations.terminate();
	_lodMeshAllocations.terminate();
	_subMeshAllocations.terminate();

	Memory::freeObjects(_meshInstances);
	Memory::freeObjects(_lodMeshInstances);
	Memory::freeObjects(_subMeshInstances);
}

MeshInstance* MeshInstancePool::allocateMeshInstance(const MeshAllocationDesc& desc) {
	const Mesh* mesh = desc._mesh;
	VirtualArray::AllocationInfo meshAllocationInfo = _meshAllocations.allocation(1);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshAllocations.allocation(mesh->_lodMeshCount);
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshAllocations.allocation(mesh->_subMeshCount);

	MeshInstance* meshInstance = &_meshInstances[meshAllocationInfo._offset];
	meshInstance->_meshAllocationInfo = meshAllocationInfo;
	meshInstance->_lodMeshAllocationInfo = lodMeshAllocationInfo;
	meshInstance->_subMeshAllocationInfo = subMeshAllocationInfo;
	meshInstance->_lodMeshInstances = &_lodMeshInstances[lodMeshAllocationInfo._offset];
	meshInstance->_subMeshInstances = &_subMeshInstances[subMeshAllocationInfo._offset];
	return meshInstance;
}

void MeshInstancePool::freeMeshInstance(MeshInstance* meshInstance) {
	_meshAllocations.freeAllocation(meshInstance->_meshAllocationInfo);
	_lodMeshAllocations.freeAllocation(meshInstance->_lodMeshAllocationInfo);
	_subMeshAllocations.freeAllocation(meshInstance->_subMeshAllocationInfo);
}

void MeshInstanceScene::initialize() {
	MeshInstancePool::InitializetionDesc desc;
	desc._meshInstanceCount = MESH_INSTANCE_CAPACITY;
	desc._lodMeshInstanceCount = LOD_MESH_INSTANCE_CAPACITY;
	desc._subMeshInstanceCount = SUB_MESH_INSTANCE_CAPACITY;
	_meshInstancePool.initialize(desc);
}

void MeshInstanceScene::terminate() {
	_meshInstancePool.terminate();
}

void MeshInstanceScene::lateUpdate() {
#define ENABLE_ZERO_CLEAR 1
	u32 destroyMeshCount = _meshInstanceDestroyInfos.getUpdateCount();
	MeshInstance** destroyMeshInstances = _meshInstanceDestroyInfos.getMeshes();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		_meshInstancePool.freeMeshInstance(destroyMeshInstances[i]);
#if ENABLE_ZERO_CLEAR
		MeshInstance* meshInstance = destroyMeshInstances[i];
		const Mesh* mesh = meshInstance->_mesh;
		memset(meshInstance->_lodMeshInstances, 0, sizeof(LodMeshInstance) * mesh->_lodMeshCount);
		memset(meshInstance->_subMeshInstances, 0, sizeof(SubMeshInstance) * mesh->_subMeshCount);
		memset(meshInstance, 0, sizeof(MeshInstance));
#endif
	}
	_meshInstanceUpdateInfos.reset();
	_meshInstanceCreateInfos.reset();
	_meshInstanceDestroyInfos.reset();
	_lodMeshInstanceUpdateInfos.reset();
	_subMeshInstanceUpdateInfos.reset();
}

MeshInstance* MeshInstanceScene::createMeshInstance(const MeshInstanceCreatationDesc& desc) {
	MeshInstancePool::MeshAllocationDesc allocationDesc;
	allocationDesc._mesh = desc._mesh;
	MeshInstance* meshInstance = _meshInstancePool.allocateMeshInstance(allocationDesc);
	_meshInstanceCreateInfos.push(meshInstance);
	return meshInstance;
}
void MeshInstanceScene::destroyMeshInstance(MeshInstance* meshInstance, u32 instanceCount) {
	_meshInstanceDestroyInfos.push(meshInstance);
}
MeshInstanceScene* MeshInstanceScene::Get() {
	return &g_meshInstanceScene;
}
}
