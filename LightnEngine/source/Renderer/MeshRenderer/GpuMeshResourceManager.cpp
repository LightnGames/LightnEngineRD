#include "GpuMeshResourceManager.h"
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Core/Memory.h>

namespace ltn {
namespace {
GpuMeshResourceManager g_gpuMeshResourceManager;
}

void GpuMeshResourceManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU 
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._sizeInByte = MeshScene::MESH_COUNT_MAX * sizeof(gpu::Mesh);
		_meshGpuBuffer.initialize(desc);

		desc._sizeInByte = MeshScene::LOD_MESH_COUNT_MAX * sizeof(gpu::LodMesh);
		_lodMeshGpuBuffer.initialize(desc);

		desc._sizeInByte = MeshScene::SUB_MESH_COUNT_MAX * sizeof(gpu::SubMesh);
		_subMeshGpuBuffer.initialize(desc);
	}

	// Descriptor
	{
		_meshDescriptors = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->allocate(3);

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_UNKNOWN;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = MeshScene::MESH_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(gpu::Mesh);
		device->createShaderResourceView(_meshGpuBuffer.getResource(), &desc, _meshDescriptors.get(0)._cpuHandle);

		desc._buffer._numElements = MeshScene::LOD_MESH_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(gpu::LodMesh);
		device->createShaderResourceView(_lodMeshGpuBuffer.getResource(), &desc, _meshDescriptors.get(1)._cpuHandle);

		desc._buffer._numElements = MeshScene::SUB_MESH_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(gpu::SubMesh);
		device->createShaderResourceView(_subMeshGpuBuffer.getResource(), &desc, _meshDescriptors.get(2)._cpuHandle);
	}
}

void GpuMeshResourceManager::terminate() {
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_meshDescriptors);
	_meshGpuBuffer.terminate();
	_lodMeshGpuBuffer.terminate();
	_subMeshGpuBuffer.terminate();
}

void GpuMeshResourceManager::update() {
	MeshScene* meshScene = MeshScene::Get();
	MeshUpdateInfos* meshUpdateInfos = meshScene->getMeshUpdateInfos();

	// 新規作成されたメッシュをGPUに追加
	u32 createdMeshCount = meshUpdateInfos->getCreatedMeshCount();
	Mesh** createdMeshes = meshUpdateInfos->getCreatedMeshes();
	for (u32 i = 0; i < createdMeshCount; ++i) {
		VramUpdater* vramUpdater = VramUpdater::Get();
		MeshPool* meshPool = meshScene->getMeshPool();
		const Mesh* mesh = createdMeshes[i];
		u32 lodMeshCount = mesh->_lodMeshCount;
		u32 subMeshCount = mesh->_subMeshCount;
		u32 meshIndex = meshPool->getMeshIndex(mesh);
		u32 lodMeshIndex = meshPool->getLodMeshIndex(mesh->_lodMeshes);
		u32 subMeshIndex = meshPool->getSubMeshIndex(mesh->_subMeshes);

		gpu::Mesh* gpuMesh = vramUpdater->enqueueUpdate<gpu::Mesh>(&_meshGpuBuffer, meshIndex, 1);
		gpu::LodMesh* gpuLodMeshes = vramUpdater->enqueueUpdate<gpu::LodMesh>(&_lodMeshGpuBuffer, lodMeshIndex, lodMeshCount);
		gpu::SubMesh* gpuSubMeshes = vramUpdater->enqueueUpdate<gpu::SubMesh>(&_subMeshGpuBuffer, subMeshIndex, subMeshCount);

		gpuMesh->_lodMeshCount = mesh->_lodMeshCount;
		gpuMesh->_lodMeshOffset = lodMeshIndex;
		gpuMesh->_streamedLodLevel = 0;

		for (u32 lodIndex = 0; lodIndex < lodMeshCount; ++lodIndex) {
			const LodMesh& lodMesh = mesh->_lodMeshes[lodIndex];
			gpu::LodMesh& gpuLodMesh = gpuLodMeshes[lodIndex];
			gpuLodMesh._subMeshOffset = lodMesh._subMeshOffset;
			gpuLodMesh._subMeshCount = lodMesh._subMeshCount;
		}

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMesh& subMesh = mesh->_subMeshes[subMeshIndex];
			gpu::SubMesh& gpuSubMesh = gpuSubMeshes[subMeshIndex];
			gpuSubMesh._indexCount = subMesh._indexCount;
			gpuSubMesh._indexOffset = subMesh._indexOffset;
		}
	}

	// 削除されたメッシュをGPUから削除
	u32 deletedMeshCount = meshUpdateInfos->getDeletedMeshCount();
	Mesh** deletedMeshes = meshUpdateInfos->getDeletedMeshes();
	for (u32 i = 0; i < deletedMeshCount; ++i) {
		VramUpdater* vramUpdater = VramUpdater::Get();
		MeshPool* meshPool = meshScene->getMeshPool();
		const Mesh* mesh = createdMeshes[i];
		u32 lodMeshCount = mesh->_lodMeshCount;
		u32 subMeshCount = mesh->_subMeshCount;
		u32 meshIndex = meshPool->getMeshIndex(mesh);
		u32 lodMeshIndex = meshPool->getLodMeshIndex(mesh->_lodMeshes);
		u32 subMeshIndex = meshPool->getSubMeshIndex(mesh->_subMeshes);

		// メモリクリアをアップロード
		gpu::Mesh* gpuMesh = vramUpdater->enqueueUpdate<gpu::Mesh>(&_meshGpuBuffer, meshIndex, 1);
		gpu::LodMesh* gpuLodMeshes = vramUpdater->enqueueUpdate<gpu::LodMesh>(&_lodMeshGpuBuffer, lodMeshIndex, lodMeshCount);
		gpu::SubMesh* gpuSubMeshes = vramUpdater->enqueueUpdate<gpu::SubMesh>(&_subMeshGpuBuffer, subMeshIndex, subMeshCount);
		memset(gpuMesh, 0, sizeof(gpu::Mesh));
		memset(gpuLodMeshes, 0, sizeof(gpu::LodMesh) * lodMeshCount);
		memset(gpuSubMeshes, 0, sizeof(gpu::SubMesh) * subMeshCount);
	}
}

GpuMeshResourceManager* GpuMeshResourceManager::Get() {
	return &g_gpuMeshResourceManager;
}
}
