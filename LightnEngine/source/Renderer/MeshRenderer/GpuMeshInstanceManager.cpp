#include "GpuMeshInstanceManager.h"
#include <Core/Memory.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/Material.h>
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

		desc._sizeInByte = MaterialScene::MATERIAL_CAPACITY * sizeof(u32);
		_subMeshInstanceOffsetsGpuBuffer.initialize(desc);
		_subMeshInstanceOffsetsGpuBuffer.setName("SubMeshInstanceOffsets");

		desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._sizeInByte = MeshInstanceScene::MESH_INSTANCE_CAPACITY * sizeof(u32);
		_meshInstanceIndexGpuBuffer.initialize(desc);
		_meshInstanceIndexGpuBuffer.setName("MeshInstanceIndices");
	}

	// SRV
	{
		DescriptorAllocator* descriptorAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
		_meshInstanceSrv = descriptorAllocator->allocate(3);
		_subMeshInstanceOffsetsSrv = descriptorAllocator->allocate();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._numElements = _meshInstanceGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_meshInstanceGpuBuffer.getResource(), &desc, _meshInstanceSrv.get(0)._cpuHandle);

		desc._buffer._numElements = _lodMeshInstanceGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_lodMeshInstanceGpuBuffer.getResource(), &desc, _meshInstanceSrv.get(1)._cpuHandle);

		desc._buffer._numElements = _subMeshInstanceGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_subMeshInstanceGpuBuffer.getResource(), &desc, _meshInstanceSrv.get(2)._cpuHandle);

		desc._buffer._numElements = _subMeshInstanceOffsetsGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_subMeshInstanceOffsetsGpuBuffer.getResource(), &desc, _subMeshInstanceOffsetsSrv._cpuHandle);
	}

	{
		_subMeshInstanceCounts = Memory::allocObjects<u32>(MaterialScene::MATERIAL_CAPACITY);
		_subMeshInstanceOffsets = Memory::allocObjects<u32>(MaterialScene::MATERIAL_CAPACITY);
		memset(_subMeshInstanceCounts, 0, MaterialScene::MATERIAL_CAPACITY);
		memset(_subMeshInstanceOffsets, 0, MaterialScene::MATERIAL_CAPACITY);
	}

	{
		VramUpdater* vramUpdater = VramUpdater::Get();
		u32* indices = vramUpdater->enqueueUpdate<u32>(&_meshInstanceIndexGpuBuffer, 0, MeshInstanceScene::MESH_INSTANCE_CAPACITY);
		for (u32 i = 0; i < MeshInstanceScene::MESH_INSTANCE_CAPACITY; ++i) {
			indices[i] = i;
		}
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
	_meshInstanceIndexGpuBuffer.terminate();

	Memory::freeObjects(_subMeshInstanceCounts);
	Memory::freeObjects(_subMeshInstanceOffsets);
}

void GpuMeshInstanceManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MeshInstanceScene* meshInstanceScene = MeshInstanceScene::Get();
	const MeshInstancePool* meshInstancePool = meshInstanceScene->getMeshInstancePool();
	MeshScene* meshScene = MeshScene::Get();
	const MeshPool* meshPool = meshScene->getMeshPool();

	bool updateSubMeshInstanceOffset = false;

	// 新規作成されたメッシュを GPU に追加
	UpdateInfos<MeshInstance>* createMeshInstanceInfos = meshInstanceScene->getMeshInstanceCreateInfos();
	u32 createMeshInstanceCount = createMeshInstanceInfos->getUpdateCount();
	auto createMeshInstances = createMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < createMeshInstanceCount; ++i) {
		const MeshInstance* meshInstance = createMeshInstances[i];
		const Mesh* mesh = meshInstance->getMesh();
		u32 lodMeshCount = mesh->getLodMeshCount();
		u32 subMeshCount = mesh->getSubMeshCount();
		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 lodMeshInstanceIndex = meshInstancePool->getLodMeshInstanceIndex(meshInstance->getLodMeshInstance());
		u32 subMeshInstanceIndex = meshInstancePool->getSubMeshInstanceIndex(meshInstance->getSubMeshInstance());

		gpu::MeshInstance* gpuMeshInstance = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceGpuBuffer, meshInstanceIndex, 1);
		gpu::LodMeshInstance* gpuLodMeshInstances = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceGpuBuffer, lodMeshInstanceIndex, lodMeshCount);
		gpu::SubMeshInstance* gpuSubMeshInstances = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceGpuBuffer, subMeshInstanceIndex, subMeshCount);

		gpuMeshInstance->_stateFlags = 1;
		gpuMeshInstance->_meshIndex = meshPool->getMeshIndex(mesh);
		gpuMeshInstance->_lodMeshInstanceOffset = lodMeshInstanceIndex;
		gpuMeshInstance->_worldMatrix = Matrix4::identity().getFloat3x4();

		for (u32 lodIndex = 0; lodIndex < lodMeshCount; ++lodIndex) {
			const LodMesh* lodMesh = mesh->getLodMesh(lodIndex);
			const LodMeshInstance* lodMeshInstance = meshInstance->getLodMeshInstance(lodIndex);
			gpu::LodMeshInstance& gpuLodMesh = gpuLodMeshInstances[lodIndex];
			gpuLodMesh._lodThreshhold = lodMeshInstance->getLodThreshold();
			gpuLodMesh._subMeshInstanceOffset = lodMeshInstanceIndex + lodMesh->_subMeshOffset;
		}

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshInstance* subMeshInstance = meshInstance->getSubMeshInstance(subMeshIndex);
			gpu::SubMeshInstance& gpuSubMesh = gpuSubMeshInstances[subMeshIndex];
			u32 materialIndex = 0;
			const u8* materialParameters = subMeshInstance->getMaterialInstance()->getMaterialParameters();
			gpuSubMesh._materialIndex = materialIndex;
			gpuSubMesh._materialParameterOffset = MaterialScene::Get()->getMaterialParameterIndex(materialParameters);
			_subMeshInstanceCounts[materialIndex]++;
			updateSubMeshInstanceOffset = true;
		}

		_meshInstanceReserveCount = Max(_meshInstanceReserveCount, meshInstanceIndex + 1);
	}

	// 更新されたメッシュインスタンス情報を GPU に更新
	UpdateInfos<MeshInstance>* updateMeshInstanceInfos = meshInstanceScene->getMeshInstanceUpdateInfos();
	u32 updateMeshInstanceCount = updateMeshInstanceInfos->getUpdateCount();
	auto updateMeshInstances = updateMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < updateMeshInstanceCount; ++i) {
		const MeshInstance* meshInstance = updateMeshInstances[i];
		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 meshInstanceOffset = sizeof(gpu::MeshInstance) * meshInstanceIndex;
		u32 worldMatrixOffset = LTN_OFFSETOF(gpu::MeshInstance, _worldMatrix);
		u32 offset = meshInstanceOffset + worldMatrixOffset;
		Float3x4* gpuWorldMatrix = reinterpret_cast<Float3x4*>(vramUpdater->enqueueUpdate(&_meshInstanceGpuBuffer, offset, sizeof(Float3x4)));
		*gpuWorldMatrix = meshInstance->getWorldMatrix().getFloat3x4();
	}

	// 削除されたメッシュを GPU から削除
	UpdateInfos<MeshInstance>* destroyMeshInstanceInfos = meshInstanceScene->getMeshInstanceDestroyInfos();
	u32 destroyMeshInstanceCount = destroyMeshInstanceInfos->getUpdateCount();
	auto destroyMeshInstances = destroyMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < destroyMeshInstanceCount; ++i) {
		const MeshInstance* meshInstance = createMeshInstances[i];
		const Mesh* mesh = meshInstance->getMesh();
		u32 lodMeshCount = mesh->getLodMeshCount();
		u32 subMeshCount = mesh->getSubMeshCount();
		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 lodMeshInstanceIndex = meshInstancePool->getLodMeshInstanceIndex(meshInstance->getLodMeshInstance());
		u32 subMeshInstanceIndex = meshInstancePool->getSubMeshInstanceIndex(meshInstance->getSubMeshInstance());

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshInstance* subMeshInstance = meshInstance->getSubMeshInstance(subMeshIndex);
			u32 materialIndex = 0;
			_subMeshInstanceCounts[materialIndex]--;
			updateSubMeshInstanceOffset = true;
		}

		// メモリクリアをアップロード
		gpu::MeshInstance* gpuMesh = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceGpuBuffer, meshInstanceIndex, 1);
		gpu::LodMeshInstance* gpuLodMeshes = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceGpuBuffer, lodMeshInstanceIndex, lodMeshCount);
		gpu::SubMeshInstance* gpuSubMeshes = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceGpuBuffer, subMeshInstanceIndex, subMeshCount);
		memset(gpuMesh, 0, sizeof(gpu::MeshInstance));
		memset(gpuLodMeshes, 0, sizeof(gpu::LodMeshInstance) * lodMeshCount);
		memset(gpuSubMeshes, 0, sizeof(gpu::SubMeshInstance) * subMeshCount);
	}

	// サブメッシュインスタンスオフセットを VRAM アップロード
	if (updateSubMeshInstanceOffset) {
		u32 offset = 0;
		for (u32 i = 0; i < MaterialScene::MATERIAL_CAPACITY; ++i) {
			_subMeshInstanceOffsets[i] = offset;
			offset += _subMeshInstanceCounts[i];
		}
		u32* offsets = vramUpdater->enqueueUpdate<u32>(&_subMeshInstanceOffsetsGpuBuffer, 0, MaterialScene::MATERIAL_CAPACITY);
		memcpy(offsets, _subMeshInstanceOffsets, sizeof(u32) * MaterialScene::MATERIAL_CAPACITY);
	}
}

rhi::VertexBufferView GpuMeshInstanceManager::getMeshInstanceIndexVertexBufferView() const {
	rhi::VertexBufferView view;
	view._bufferLocation = _meshInstanceIndexGpuBuffer.getGpuVirtualAddress();
	view._sizeInBytes = _meshInstanceIndexGpuBuffer.getSizeInByte();
	view._strideInBytes = sizeof(u32);
	return view;
}

GpuMeshInstanceManager* GpuMeshInstanceManager::Get() {
	return &g_gpuMeshInstanceManager;
}
}
