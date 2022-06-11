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
		_meshInstanceAllocations.initialize(handleDesc);

		handleDesc._size = desc._lodMeshInstanceCount;
		_lodMeshInstanceAllocations.initialize(handleDesc);

		handleDesc._size = desc._subMeshInstanceCount;
		_subMeshInstanceAllocations.initialize(handleDesc);
	}

	_meshInstances = Memory::allocObjects<MeshInstance>(desc._meshInstanceCount);
	_lodMeshInstances = Memory::allocObjects<LodMeshInstance>(desc._lodMeshInstanceCount);
	_subMeshInstances = Memory::allocObjects<SubMeshInstance>(desc._subMeshInstanceCount);
	_meshInstanceAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(desc._meshInstanceCount);
	_lodMeshInstanceAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(desc._meshInstanceCount);
	_subMeshInstanceAllocationInfos = Memory::allocObjects<VirtualArray::AllocationInfo>(desc._meshInstanceCount);
}

void MeshInstancePool::terminate() {
	_meshInstanceAllocations.terminate();
	_lodMeshInstanceAllocations.terminate();
	_subMeshInstanceAllocations.terminate();

	Memory::freeObjects(_meshInstances);
	Memory::freeObjects(_lodMeshInstances);
	Memory::freeObjects(_subMeshInstances);
	Memory::freeObjects(_meshInstanceAllocationInfos);
	Memory::freeObjects(_lodMeshInstanceAllocationInfos);
	Memory::freeObjects(_subMeshInstanceAllocationInfos);
}

MeshInstance* MeshInstancePool::allocateMeshInstance(const MeshAllocationDesc& desc) {
	const Mesh* mesh = desc._mesh;
	VirtualArray::AllocationInfo meshAllocationInfo = _meshInstanceAllocations.allocation(1);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshInstanceAllocations.allocation(mesh->getLodMeshCount());
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshInstanceAllocations.allocation(mesh->getSubMeshCount());

	_meshInstanceAllocationInfos[meshAllocationInfo._offset] = meshAllocationInfo;
	_lodMeshInstanceAllocationInfos[meshAllocationInfo._offset] = lodMeshAllocationInfo;
	_subMeshInstanceAllocationInfos[meshAllocationInfo._offset] = subMeshAllocationInfo;

	MeshInstance* meshInstance = &_meshInstances[meshAllocationInfo._offset];
	meshInstance->setLodMeshInstances(&_lodMeshInstances[lodMeshAllocationInfo._offset]);
	meshInstance->setSubMeshInstances(&_subMeshInstances[subMeshAllocationInfo._offset]);
	return meshInstance;
}

void MeshInstancePool::freeMeshInstance(const MeshInstance* meshInstance) {
	u32 meshInstanceIndex = getMeshInstanceIndex(meshInstance);
	_meshInstanceAllocations.freeAllocation(_meshInstanceAllocationInfos[meshInstanceIndex]);
	_lodMeshInstanceAllocations.freeAllocation(_lodMeshInstanceAllocationInfos[meshInstanceIndex]);
	_subMeshInstanceAllocations.freeAllocation(_subMeshInstanceAllocationInfos[meshInstanceIndex]);
}

void MeshInstanceScene::initialize() {
	MeshInstancePool::InitializetionDesc desc;
	desc._meshInstanceCount = MESH_INSTANCE_CAPACITY;
	desc._lodMeshInstanceCount = LOD_MESH_INSTANCE_CAPACITY;
	desc._subMeshInstanceCount = SUB_MESH_INSTANCE_CAPACITY;
	_meshInstancePool.initialize(desc);
}

void MeshInstanceScene::terminate() {
	lateUpdate();
	_meshInstancePool.terminate();
}

void MeshInstanceScene::lateUpdate() {
#define ENABLE_ZERO_CLEAR 1
	u32 destroyMeshCount = _meshInstanceDestroyInfos.getUpdateCount();
	auto destroyMeshInstances = _meshInstanceDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		_meshInstancePool.freeMeshInstance(destroyMeshInstances[i]);
#if ENABLE_ZERO_CLEAR
		u32 meshInstanceIndex = _meshInstancePool.getMeshInstanceIndex(destroyMeshInstances[i]);
		MeshInstance* meshInstance = _meshInstancePool.getMeshInstance(meshInstanceIndex);
		const Mesh* mesh = meshInstance->getMesh();
		memset(meshInstance->getLodMeshInstance(0), 0, sizeof(LodMeshInstance) * mesh->getLodMeshCount());
		memset(meshInstance->getSubMeshInstance(0), 0, sizeof(SubMeshInstance) * mesh->getSubMeshCount());
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
	meshInstance->setMesh(desc._mesh);
	meshInstance->setWorldMatrix(Matrix4::identity(), false);

	_meshInstanceCreateInfos.push(meshInstance);
	return meshInstance;
}

void MeshInstanceScene::destroyMeshInstance(MeshInstance* meshInstance) {
	_meshInstanceDestroyInfos.push(meshInstance);
}

void MeshInstance::setMaterialInstance(u16 materialSlotIndex, MaterialInstance* materialInstance) {
	UpdateInfos<SubMeshInstance>* subMeshInstanceUpdateInfos = MeshInstanceScene::Get()->getSubMeshInstanceUpdateInfos();
	const SubMesh* subMeshes = _mesh->getSubMesh();
	u32 subMeshCount = _mesh->getSubMeshCount();
	for (u32 i = 0; i < subMeshCount; ++i) {
		if (subMeshes[i]._materialSlotIndex == materialSlotIndex) {
			_subMeshInstances[i].setMaterialInstance(materialInstance);
			subMeshInstanceUpdateInfos->push(&_subMeshInstances[i]);
		}
	}
}

void MeshInstance::setMaterialInstance(u64 materialSlotNameHash, MaterialInstance* materialInstance) {
	const u64* materialSlotNameHashes = _mesh->getMaterialSlotNameHashes();
	u32 materialSlotCount = _mesh->getMaterialSlotCount();
	u16 findMaterialSlotIndex = 0;
	for (u16 i = 0; i < materialSlotCount; ++i) {
		if (materialSlotNameHashes[i] == materialSlotNameHash) {
			findMaterialSlotIndex = i;
			break;
		}
	}

	setMaterialInstance(findMaterialSlotIndex, materialInstance);
}

void MeshInstance::setWorldMatrix(const Matrix4& worldMatrix, bool dirtyFlags) {
	_worldMatrix = worldMatrix;
	if (dirtyFlags) {
		MeshInstanceScene::Get()->getMeshInstanceUpdateInfos()->push(this);
	}
}

MeshInstanceScene* MeshInstanceScene::Get() {
	return &g_meshInstanceScene;
}
}
