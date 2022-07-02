#include "GpuMeshResourceManager.h"
#include <RendererScene/MeshGeometry.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>

namespace ltn {
namespace {
GpuMeshResourceManager g_gpuMeshResourceManager;
}

void GpuMeshResourceManager::initialize() {
	CpuScopedPerf scopedPerf("GpuMeshResourceManager");
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU 
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._sizeInByte = MeshGeometryScene::MESH_GEOMETRY_CAPACITY * sizeof(gpu::Mesh);
		_meshGpuBuffer.initialize(desc);
		_meshGpuBuffer.setName("Meshes");

		desc._sizeInByte = MeshGeometryScene::LOD_MESH_GEOMETRY_CAPACITY * sizeof(gpu::LodMesh);
		_lodMeshGpuBuffer.initialize(desc);
		_lodMeshGpuBuffer.setName("LodMeshes");

		desc._sizeInByte = MeshGeometryScene::SUB_MESH_GEOMETRY_CAPACITY * sizeof(gpu::SubMesh);
		_subMeshGpuBuffer.initialize(desc);
		_subMeshGpuBuffer.setName("SubMeshes");
	}

	// SRV
	{
		_meshSrv = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->allocate(3);

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._numElements = _meshGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_meshGpuBuffer.getResource(), &desc, _meshSrv.get(0)._cpuHandle);

		desc._buffer._numElements = _lodMeshGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_lodMeshGpuBuffer.getResource(), &desc, _meshSrv.get(1)._cpuHandle);

		desc._buffer._numElements = _subMeshGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_subMeshGpuBuffer.getResource(), &desc, _meshSrv.get(2)._cpuHandle);
	}
}

void GpuMeshResourceManager::terminate() {
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_meshSrv);
	_meshGpuBuffer.terminate();
	_lodMeshGpuBuffer.terminate();
	_subMeshGpuBuffer.terminate();
}

void GpuMeshResourceManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MeshGeometryScene* meshScene = MeshGeometryScene::Get();
	const MeshGeometryPool* meshPool = meshScene->getMeshGeometryPool();

	// 新規作成されたメッシュをGPUに追加
	const UpdateInfos<MeshGeometry>* meshCreateInfos = meshScene->getCreateInfos();
	u32 createMeshCount = meshCreateInfos->getUpdateCount();
	auto createMeshes = meshCreateInfos->getObjects();
	for (u32 i = 0; i < createMeshCount; ++i) {
		const MeshGeometry* mesh = createMeshes[i];
		u32 lodMeshCount = mesh->getLodMeshCount();
		u32 subMeshCount = mesh->getSubMeshCount();
		u32 meshIndex = meshPool->getMeshGeometryIndex(mesh);
		u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(mesh->getLodMesh());
		u32 subMeshIndex = meshPool->getSubMeshGeometryIndex(mesh->getSubMesh());

		gpu::Mesh* gpuMesh = vramUpdater->enqueueUpdate<gpu::Mesh>(&_meshGpuBuffer, meshIndex, 1);
		gpu::LodMesh* gpuLodMeshes = vramUpdater->enqueueUpdate<gpu::LodMesh>(&_lodMeshGpuBuffer, lodMeshIndex, lodMeshCount);
		gpu::SubMesh* gpuSubMeshes = vramUpdater->enqueueUpdate<gpu::SubMesh>(&_subMeshGpuBuffer, subMeshIndex, subMeshCount);

		gpuMesh->_lodMeshCount = mesh->getLodMeshCount();
		gpuMesh->_lodMeshOffset = lodMeshIndex;

		for (u32 lodIndex = 0; lodIndex < lodMeshCount; ++lodIndex) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(lodIndex);
			gpu::LodMesh& gpuLodMesh = gpuLodMeshes[lodIndex];
			gpuLodMesh._subMeshOffset = subMeshIndex + lodMesh->_subMeshOffset;
			gpuLodMesh._subMeshCount = lodMesh->_subMeshCount;
		}

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshGeometry* subMesh = mesh->getSubMesh(subMeshIndex);
			gpu::SubMesh& gpuSubMesh = gpuSubMeshes[subMeshIndex];
			gpuSubMesh._indexCount = subMesh->_indexCount;
			gpuSubMesh._indexOffset = subMesh->_indexOffset;
		}
	}

	// 削除されたメッシュをGPUから削除
	const UpdateInfos<MeshGeometry>* meshDestroyInfos = meshScene->getDestroyInfos();
	u32 destroyMeshCount = meshDestroyInfos->getUpdateCount();
	auto destroyMeshes = meshDestroyInfos->getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		const MeshGeometry* mesh = destroyMeshes[i];
		u32 lodMeshCount = mesh->getLodMeshCount();
		u32 subMeshCount = mesh->getSubMeshCount();
		u32 meshIndex = meshPool->getMeshGeometryIndex(mesh);
		u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(mesh->getLodMesh());
		u32 subMeshIndex = meshPool->getSubMeshGeometryIndex(mesh->getSubMesh());

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
