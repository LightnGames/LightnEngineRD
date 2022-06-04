#include "GpuMeshInstanceManager.h"
#include <Core/Memory.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>

namespace ltn {
namespace {
GpuMeshInstanceManager g_gpuMeshInstanceManager;
}
void GpuMeshInstanceManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU 
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._sizeInByte = MeshInstanceScene::MESH_INSTANCE_CAPACITY * sizeof(gpu::MeshInstance);
		_meshInstanceGpuBuffer.initialize(desc);
		_meshInstanceGpuBuffer.setName("MeshInstances");

		desc._sizeInByte = MeshInstanceScene::LOD_MESH_INSTANCE_CAPACITY * sizeof(gpu::LodMeshInstance);
		_lodMeshInstanceGpuBuffer.initialize(desc);
		_lodMeshInstanceGpuBuffer.setName("LodMeshInstances");

		desc._sizeInByte = MeshInstanceScene::SUB_MESH_INSTANCE_CAPACITY * sizeof(gpu::SubMeshInstance);
		_subMeshInstanceGpuBuffer.initialize(desc);
		_subMeshInstanceGpuBuffer.setName("SubMeshInstances");

		desc._sizeInByte = MeshInstanceScene::SUB_MESH_INSTANCE_CAPACITY * sizeof(u32);
		_subMeshInstanceOffsetsGpuBuffer.initialize(desc);
		_subMeshInstanceOffsetsGpuBuffer.setName("SubMeshInstanceOffsets");
	}

	// Descriptor
	{
		DescriptorAllocator* descriptorAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
		_meshInstanceSrv = descriptorAllocator->allocate(3);
		_subMeshInstanceOffsetsSrv = descriptorAllocator->allocate();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_UNKNOWN;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = MeshInstanceScene::MESH_INSTANCE_CAPACITY;
		desc._buffer._structureByteStride = sizeof(gpu::MeshInstance);
		device->createShaderResourceView(_meshInstanceGpuBuffer.getResource(), &desc, _meshInstanceSrv.get(0)._cpuHandle);

		desc._buffer._numElements = MeshInstanceScene::LOD_MESH_INSTANCE_CAPACITY;
		desc._buffer._structureByteStride = sizeof(gpu::LodMeshInstance);
		device->createShaderResourceView(_lodMeshInstanceGpuBuffer.getResource(), &desc, _meshInstanceSrv.get(1)._cpuHandle);

		desc._buffer._numElements = MeshInstanceScene::SUB_MESH_INSTANCE_CAPACITY;
		desc._buffer._structureByteStride = sizeof(gpu::SubMeshInstance);
		device->createShaderResourceView(_subMeshInstanceGpuBuffer.getResource(), &desc, _meshInstanceSrv.get(2)._cpuHandle);

		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._structureByteStride = 0;
		device->createShaderResourceView(_subMeshInstanceOffsetsGpuBuffer.getResource(), &desc, _subMeshInstanceOffsetsSrv._cpuHandle);
	}

	{
		_subMeshInstanceCounts = Memory::allocObjects<u32>(MeshScene::SUB_MESH_CAPACITY);
		_subMeshInstanceOffsets = Memory::allocObjects<u32>(MeshScene::SUB_MESH_CAPACITY);
		memset(_subMeshInstanceCounts, 0, MeshScene::SUB_MESH_CAPACITY);
		memset(_subMeshInstanceOffsets, 0, MeshScene::SUB_MESH_CAPACITY);
	}
}
void GpuMeshInstanceManager::terminate() {
	DescriptorAllocator* descriptorAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
	descriptorAllocator->free(_meshInstanceSrv);
	descriptorAllocator->free(_subMeshInstanceOffsetsSrv);
	_meshInstanceGpuBuffer.terminate();
	_lodMeshInstanceGpuBuffer.terminate();
	_subMeshInstanceGpuBuffer.terminate();
	_subMeshInstanceOffsetsGpuBuffer.terminate();

	Memory::freeObjects(_subMeshInstanceCounts);
	Memory::freeObjects(_subMeshInstanceOffsets);
}

void GpuMeshInstanceManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MeshInstanceScene* meshInstanceScene = MeshInstanceScene::Get();
	const MeshInstancePool* meshInstancePool = meshInstanceScene->getMeshInstancePool();
	MeshScene* meshScene = MeshScene::Get();
	const MeshPool* meshPool = meshScene->getMeshPool();

	// 新規作成されたメッシュをGPUに追加
	UpdateInfos<MeshInstance>* createdMeshInstanceInfos = meshInstanceScene->getMeshInstanceCreateInfos();
	u32 createdMeshCount = createdMeshInstanceInfos->getUpdateCount();
	auto createdMeshes = createdMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < createdMeshCount; ++i) {
		const MeshInstance* meshInstance = createdMeshes[i];
		const Mesh* mesh = meshInstance->_mesh;
		u32 lodMeshCount = mesh->_lodMeshCount;
		u32 subMeshCount = mesh->_subMeshCount;
		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 lodMeshInstanceIndex = meshInstancePool->getLodMeshInstanceIndex(meshInstance->_lodMeshInstances);
		u32 subMeshInstanceIndex = meshInstancePool->getSubMeshInstanceIndex(meshInstance->_subMeshInstances);

		gpu::MeshInstance* gpuMeshInstance = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceGpuBuffer, meshInstanceIndex, 1);
		gpu::LodMeshInstance* gpuLodMeshInstances = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceGpuBuffer, lodMeshInstanceIndex, lodMeshCount);
		gpu::SubMeshInstance* gpuSubMeshInstances = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceGpuBuffer, subMeshInstanceIndex, subMeshCount);

		gpuMeshInstance->_meshIndex = meshPool->getMeshIndex(mesh);
		gpuMeshInstance->_lodMeshInstanceOffset = lodMeshInstanceIndex;

		for (u32 lodIndex = 0; lodIndex < lodMeshCount; ++lodIndex) {
			const LodMesh& lodMesh = mesh->_lodMeshes[lodIndex];
			const LodMeshInstance& lodMeshInstance = meshInstance->_lodMeshInstances[lodIndex];
			gpu::LodMeshInstance& gpuLodMesh = gpuLodMeshInstances[lodIndex];
			gpuLodMesh._lodThreshhold = lodMeshInstance._lodThreshold;
			gpuLodMesh._subMeshInstanceOffset = lodMeshInstanceIndex + lodMesh._subMeshOffset;
		}

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshInstance& subMeshInstance = meshInstance->_subMeshInstances[subMeshIndex];
			gpu::SubMeshInstance& gpuSubMesh = gpuSubMeshInstances[subMeshIndex];
			gpuSubMesh._materialIndex = 0;
		}
	}

	// 削除されたメッシュをGPUから削除
	UpdateInfos<MeshInstance>* destroyMeshInstanceInfos = meshInstanceScene->getMeshInstanceDestroyInfos();
	u32 destroyMeshCount = destroyMeshInstanceInfos->getUpdateCount();
	auto destroyMeshes = destroyMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		const MeshInstance* meshInstance = createdMeshes[i];
		const Mesh* mesh = meshInstance->_mesh;
		u32 lodMeshCount = mesh->_lodMeshCount;
		u32 subMeshCount = mesh->_subMeshCount;
		u32 meshIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 lodMeshIndex = meshInstancePool->getLodMeshInstanceIndex(meshInstance->_lodMeshInstances);
		u32 subMeshIndex = meshInstancePool->getSubMeshInstanceIndex(meshInstance->_subMeshInstances);

		// メモリクリアをアップロード
		gpu::MeshInstance* gpuMesh = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceGpuBuffer, meshIndex, 1);
		gpu::LodMeshInstance* gpuLodMeshes = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceGpuBuffer, lodMeshIndex, lodMeshCount);
		gpu::SubMeshInstance* gpuSubMeshes = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceGpuBuffer, subMeshIndex, subMeshCount);
		memset(gpuMesh, 0, sizeof(gpu::MeshInstance));
		memset(gpuLodMeshes, 0, sizeof(gpu::LodMeshInstance) * lodMeshCount);
		memset(gpuSubMeshes, 0, sizeof(gpu::SubMeshInstance) * subMeshCount);
	}
}

GpuMeshInstanceManager* GpuMeshInstanceManager::Get() {
	return &g_gpuMeshInstanceManager;
}
}
