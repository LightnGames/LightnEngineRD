#include "MeshInstance.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>
#include <Core/Aabb.h>
#include <Renderer/RenderCore/DebugRenderer.h>
#include <RendererScene/MeshGeometry.h>
#include <Renderer/RenderCore/ImGuiSystem.h>

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

	_chunkAllocator.alloc([this, desc](ChunkAllocator::Allocation& allocation) {
		_meshInstances = allocation.allocateObjects<MeshInstance>(desc._meshInstanceCount);
		_lodMeshInstances = allocation.allocateObjects<LodMeshInstance>(desc._lodMeshInstanceCount);
		_subMeshInstances = allocation.allocateObjects<SubMeshInstance>(desc._subMeshInstanceCount);
		_meshInstanceAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(desc._meshInstanceCount);
		_lodMeshInstanceAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(desc._meshInstanceCount);
		_subMeshInstanceAllocationInfos = allocation.allocateObjects<VirtualArray::AllocationInfo>(desc._meshInstanceCount);
		_meshInstanceEnabledFlags = allocation.allocateClearedObjects<u8>(desc._meshInstanceCount);
	});
}

void MeshInstancePool::terminate() {
	_meshInstanceAllocations.terminate();
	_lodMeshInstanceAllocations.terminate();
	_subMeshInstanceAllocations.terminate();

	_chunkAllocator.freeChunk();
}

MeshInstance* MeshInstancePool::allocateMeshInstances(const MeshAllocationDesc& desc, u32 instanceCount) {
	const MeshGeometry* mesh = desc._mesh;
	u32 lodMeshCount = mesh->getLodMeshCount();
	u32 subMeshCount = mesh->getSubMeshCount();
	VirtualArray::AllocationInfo meshAllocationInfo = _meshInstanceAllocations.allocation(instanceCount);
	VirtualArray::AllocationInfo lodMeshAllocationInfo = _lodMeshInstanceAllocations.allocation(lodMeshCount * instanceCount);
	VirtualArray::AllocationInfo subMeshAllocationInfo = _subMeshInstanceAllocations.allocation(subMeshCount * instanceCount);

	_meshInstanceAllocationInfos[meshAllocationInfo._offset] = meshAllocationInfo;
	_lodMeshInstanceAllocationInfos[meshAllocationInfo._offset] = lodMeshAllocationInfo;
	_subMeshInstanceAllocationInfos[meshAllocationInfo._offset] = subMeshAllocationInfo;

	for (u32 i = 0; i < instanceCount; ++i) {
		u32 meshInstanceIndex = u32(meshAllocationInfo._offset) + i;
		MeshInstance* meshInstance = &_meshInstances[meshInstanceIndex];
		meshInstance->setLodMeshInstances(&_lodMeshInstances[lodMeshAllocationInfo._offset + lodMeshCount * i]);
		meshInstance->setSubMeshInstances(&_subMeshInstances[subMeshAllocationInfo._offset + subMeshCount * i]);
		_meshInstanceEnabledFlags[meshInstanceIndex] = 1;
	}

	return &_meshInstances[meshAllocationInfo._offset];
}

void MeshInstancePool::freeMeshInstances(const MeshInstance* meshInstances) {
	u32 meshInstanceIndex = getMeshInstanceIndex(meshInstances);
	LTN_ASSERT(_meshInstanceAllocationInfos[meshInstanceIndex]._handle != 0);

	_meshInstanceAllocations.freeAllocation(_meshInstanceAllocationInfos[meshInstanceIndex]);
	_lodMeshInstanceAllocations.freeAllocation(_lodMeshInstanceAllocationInfos[meshInstanceIndex]);
	_subMeshInstanceAllocations.freeAllocation(_subMeshInstanceAllocationInfos[meshInstanceIndex]);
	_meshInstanceEnabledFlags[meshInstanceIndex] = 0;
}

void MeshInstanceScene::initialize() {
	CpuScopedPerf scopedPerf("MeshInstanceScene");
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
		_meshInstancePool.freeMeshInstances(destroyMeshInstances[i]);
#if ENABLE_ZERO_CLEAR
		u32 meshInstanceIndex = _meshInstancePool.getMeshInstanceIndex(destroyMeshInstances[i]);
		MeshInstance* meshInstance = _meshInstancePool.getMeshInstance(meshInstanceIndex);
		const MeshGeometry* mesh = meshInstance->getMesh();
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

	ImGui::Begin("MeshInstance");
	static bool showBounds = false;
	ImGui::Checkbox("Show Bounds", &showBounds);
	if (showBounds) {
		DebugRenderer* debugRenderer = DebugRenderer::Get();
		const u8* enabledFlags = _meshInstancePool.getEnabledFlags();
		for (u32 i = 0; i < MeshInstanceScene::MESH_INSTANCE_CAPACITY; ++i) {
			if (enabledFlags[i] == 0) {
				continue;
			}

			const MeshInstance* meshInstance = _meshInstancePool.getMeshInstance(i);
			const MeshGeometry* mesh = meshInstance->getMesh();
			AABB boundsAabb = AABB(mesh->getBoundsMin(), mesh->getBoundsMax()).getTransformedAabb(meshInstance->getWorldMatrix());
			debugRenderer->drawAabb(boundsAabb._min, boundsAabb._max, Color::Blue());
		}
	}
	ImGui::End();
}

MeshInstance* MeshInstanceScene::createMeshInstances(const CreatationDesc& desc, u32 instanceCount) {
	MeshInstancePool::MeshAllocationDesc allocationDesc;
	allocationDesc._mesh = desc._mesh;

	MeshInstance* meshInstances = _meshInstancePool.allocateMeshInstances(allocationDesc, instanceCount);
	for (u32 i = 0; i < instanceCount; ++i) {
		MeshInstance* meshInstance = &meshInstances[i];
		meshInstance->setMesh(desc._mesh);
		meshInstance->setWorldMatrix(Matrix4::identity(), false);
		u32 lodCount = desc._mesh->getLodMeshCount();
		for (u32 lodLevel = 0; lodLevel < lodCount; ++lodLevel) {
			f32 threshhold = 1.0f - ((lodLevel + 1) / f32(lodCount));
			meshInstance->getLodMeshInstance(lodLevel)->setLodThreshold(threshhold * 0.01f);
		}
		_meshInstanceCreateInfos.push(meshInstance);
	}

	return meshInstances;
}

void MeshInstanceScene::destroyMeshInstances(MeshInstance* meshInstances) {
	_meshInstanceDestroyInfos.push(meshInstances);
}

void MeshInstance::setMaterial(u16 materialSlotIndex, const Material* material) {
	//UpdateInfos<SubMeshInstance>* subMeshInstanceUpdateInfos = MeshInstanceScene::Get()->getSubMeshInstanceUpdateInfos();
	const SubMeshGeometry* subMeshes = _mesh->getSubMesh();
	u32 subMeshCount = _mesh->getSubMeshCount();
	for (u32 i = 0; i < subMeshCount; ++i) {
		if (subMeshes[i]._materialSlotIndex == materialSlotIndex) {
			_subMeshInstances[i].setMaterial(material);
			//subMeshInstanceUpdateInfos->push(&_subMeshInstances[i]);
		}
	}
}

void MeshInstance::setMaterial(u64 materialSlotNameHash, const Material* material) {
	const u64* materialSlotNameHashes = _mesh->getMaterialSlotNameHashes();
	u32 materialSlotCount = _mesh->getMaterialSlotCount();
	u16 findMaterialSlotIndex = 0;
	for (u16 i = 0; i < materialSlotCount; ++i) {
		if (materialSlotNameHashes[i] == materialSlotNameHash) {
			findMaterialSlotIndex = i;
			break;
		}
	}

	setMaterial(findMaterialSlotIndex, material);
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
