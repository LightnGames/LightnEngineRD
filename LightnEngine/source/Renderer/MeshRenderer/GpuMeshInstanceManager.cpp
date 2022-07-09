#include "GpuMeshInstanceManager.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>
#include <Core/Aabb.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/Material.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>

namespace ltn {
namespace {
GpuMeshInstanceManager g_gpuMeshInstanceManager;
}
void GpuMeshInstanceManager::initialize() {
	CpuScopedPerf scopedPerf("GpuMeshInstanceManager");
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

		desc._sizeInByte = PipelineSetScene::PIPELINE_SET_CAPACITY * sizeof(u32);
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
		_pipelineSetSubMeshInstanceCounts = Memory::allocObjects<u32>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		_pipelineSetSubMeshInstanceOffsets = Memory::allocObjects<u32>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		memset(_pipelineSetSubMeshInstanceCounts, 0, PipelineSetScene::PIPELINE_SET_CAPACITY);
		memset(_pipelineSetSubMeshInstanceOffsets, 0, PipelineSetScene::PIPELINE_SET_CAPACITY);
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

	Memory::freeObjects(_pipelineSetSubMeshInstanceCounts);
	Memory::freeObjects(_pipelineSetSubMeshInstanceOffsets);
}

void GpuMeshInstanceManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	PipelineSetScene* pipelineSetScene = PipelineSetScene::Get();
	MaterialScene* materialScene = MaterialScene::Get();
	MaterialParameterContainer* materialParameterContainer = MaterialParameterContainer::Get();
	MeshInstanceScene* meshInstanceScene = MeshInstanceScene::Get();
	const MeshInstancePool* meshInstancePool = meshInstanceScene->getMeshInstancePool();
	MeshGeometryScene* meshScene = MeshGeometryScene::Get();
	const MeshGeometryPool* meshPool = meshScene->getMeshGeometryPool();

	bool updatePipelineSetOffset = false;

	// �V�K�쐬���ꂽ���b�V���� GPU �ɒǉ�
	UpdateInfos<MeshInstance>* createMeshInstanceInfos = meshInstanceScene->getMeshInstanceCreateInfos();
	u32 createMeshInstanceCount = createMeshInstanceInfos->getUpdateCount();
	auto createMeshInstances = createMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < createMeshInstanceCount; ++i) {
		const MeshInstance* meshInstance = createMeshInstances[i];
		const MeshGeometry* mesh = meshInstance->getMesh();
		u32 lodMeshCount = mesh->getLodMeshCount();
		u32 subMeshCount = mesh->getSubMeshCount();
		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 lodMeshInstanceIndex = meshInstancePool->getLodMeshInstanceIndex(meshInstance->getLodMeshInstance());
		u32 subMeshInstanceIndex = meshInstancePool->getSubMeshInstanceIndex(meshInstance->getSubMeshInstance());

		gpu::MeshInstance* gpuMeshInstance = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceGpuBuffer, meshInstanceIndex, 1);
		gpu::LodMeshInstance* gpuLodMeshInstances = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceGpuBuffer, lodMeshInstanceIndex, lodMeshCount);
		gpu::SubMeshInstance* gpuSubMeshInstances = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceGpuBuffer, subMeshInstanceIndex, subMeshCount);

		gpuMeshInstance->_stateFlags = 1;
		gpuMeshInstance->_meshIndex = meshPool->getMeshGeometryIndex(mesh);
		gpuMeshInstance->_lodMeshInstanceOffset = lodMeshInstanceIndex;

		gpu::MeshInstanceDynamicData& dynamicData = gpuMeshInstance->_dynamicData;
		dynamicData._worldMatrix = Matrix4::identity().getFloat3x4();
		dynamicData._aabbMin = mesh->getBoundsMin().getFloat3();
		dynamicData._aabbMax = mesh->getBoundsMax().getFloat3();
		dynamicData._boundsRadius = (mesh->getBoundsMax() - mesh->getBoundsMin()).length();

		for (u32 lodIndex = 0; lodIndex < lodMeshCount; ++lodIndex) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(lodIndex);
			const LodMeshInstance* lodMeshInstance = meshInstance->getLodMeshInstance(lodIndex);
			gpu::LodMeshInstance& gpuLodMesh = gpuLodMeshInstances[lodIndex];
			gpuLodMesh._lodThreshhold = lodMeshInstance->getLodThreshold();
			gpuLodMesh._subMeshInstanceOffset = subMeshInstanceIndex + lodMesh->_subMeshOffset;
		}

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshInstance* subMeshInstance = meshInstance->getSubMeshInstance(subMeshIndex);
			const Material* material = subMeshInstance->getMaterial();
			gpu::SubMeshInstance& gpuSubMeshInstance = gpuSubMeshInstances[subMeshIndex];
			u32 materialIndex = materialScene->getMaterialIndex(material);
			const u8* materialParameters = material->getParameterSet()->_parameters;
			gpuSubMeshInstance._materialIndex = materialIndex;
			gpuSubMeshInstance._materialParameterOffset = materialParameterContainer->getMaterialParameterIndex(materialParameters);

			u32 pipelineSetIndex = pipelineSetScene->getPipelineSetIndex(material->getPipelineSet());
			_pipelineSetSubMeshInstanceCounts[pipelineSetIndex]++;
			updatePipelineSetOffset = true;
		}

		_meshInstanceReserveCount = Max(_meshInstanceReserveCount, meshInstanceIndex + 1);
	}

	// �X�V���ꂽ���b�V���C���X�^���X���� GPU �ɍX�V
	UpdateInfos<MeshInstance>* updateMeshInstanceInfos = meshInstanceScene->getMeshInstanceUpdateInfos();
	u32 updateMeshInstanceCount = updateMeshInstanceInfos->getUpdateCount();
	auto updateMeshInstances = updateMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < updateMeshInstanceCount; ++i) {
		const MeshInstance* meshInstance = updateMeshInstances[i];

		const MeshGeometry* mesh = meshInstance->getMesh();
		AABB boundsAabb = AABB(mesh->getBoundsMin(), mesh->getBoundsMax()).getTransformedAabb(meshInstance->getWorldMatrix());

		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 meshInstanceOffset = sizeof(gpu::MeshInstance) * meshInstanceIndex;
		u32 dynamicDataOffset = LTN_OFFSETOF(gpu::MeshInstance, _dynamicData);
		u32 offset = meshInstanceOffset + dynamicDataOffset;
		gpu::MeshInstanceDynamicData* dynamicData = reinterpret_cast<gpu::MeshInstanceDynamicData*>(vramUpdater->enqueueUpdate(&_meshInstanceGpuBuffer, offset, sizeof(gpu::MeshInstanceDynamicData)));
		dynamicData->_aabbMin = boundsAabb._min.getFloat3();
		dynamicData->_aabbMax = boundsAabb._max.getFloat3();
		dynamicData->_worldMatrix = meshInstance->getWorldMatrix().getFloat3x4();
		dynamicData->_boundsRadius = (mesh->getBoundsMax() - mesh->getBoundsMin()).length();
	}

	// �폜���ꂽ���b�V���� GPU ����폜
	UpdateInfos<MeshInstance>* destroyMeshInstanceInfos = meshInstanceScene->getMeshInstanceDestroyInfos();
	u32 destroyMeshInstanceCount = destroyMeshInstanceInfos->getUpdateCount();
	auto destroyMeshInstances = destroyMeshInstanceInfos->getObjects();
	for (u32 i = 0; i < destroyMeshInstanceCount; ++i) {
		const MeshInstance* meshInstance = destroyMeshInstances[i];
		const MeshGeometry* mesh = meshInstance->getMesh();
		u32 lodMeshCount = mesh->getLodMeshCount();
		u32 subMeshCount = mesh->getSubMeshCount();
		u32 meshInstanceIndex = meshInstancePool->getMeshInstanceIndex(meshInstance);
		u32 lodMeshInstanceIndex = meshInstancePool->getLodMeshInstanceIndex(meshInstance->getLodMeshInstance());
		u32 subMeshInstanceIndex = meshInstancePool->getSubMeshInstanceIndex(meshInstance->getSubMeshInstance());

		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshInstance* subMeshInstance = meshInstance->getSubMeshInstance(subMeshIndex);
			const Material* material = subMeshInstance->getMaterial();
			u32 pipelineSetIndex = pipelineSetScene->getPipelineSetIndex(material->getPipelineSet());
			_pipelineSetSubMeshInstanceCounts[pipelineSetIndex]--;
			updatePipelineSetOffset = true;
		}

		// �������N���A���A�b�v���[�h
		gpu::MeshInstance* gpuMesh = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceGpuBuffer, meshInstanceIndex, 1);
		gpu::LodMeshInstance* gpuLodMeshes = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceGpuBuffer, lodMeshInstanceIndex, lodMeshCount);
		gpu::SubMeshInstance* gpuSubMeshes = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceGpuBuffer, subMeshInstanceIndex, subMeshCount);
		memset(gpuMesh, 0, sizeof(gpu::MeshInstance));
		memset(gpuLodMeshes, 0, sizeof(gpu::LodMeshInstance) * lodMeshCount);
		memset(gpuSubMeshes, 0, sizeof(gpu::SubMeshInstance) * subMeshCount);
	}

	// �T�u���b�V���C���X�^���X�I�t�Z�b�g�� VRAM �A�b�v���[�h
	if (updatePipelineSetOffset) {
		u32 offset = 0;
		for (u32 i = 0; i < PipelineSetScene::PIPELINE_SET_CAPACITY; ++i) {
			_pipelineSetSubMeshInstanceOffsets[i] = offset;
			offset += _pipelineSetSubMeshInstanceCounts[i];
		}
		u32* offsets = vramUpdater->enqueueUpdate<u32>(&_subMeshInstanceOffsetsGpuBuffer, 0, PipelineSetScene::PIPELINE_SET_CAPACITY);
		memcpy(offsets, _pipelineSetSubMeshInstanceOffsets, sizeof(u32) * PipelineSetScene::PIPELINE_SET_CAPACITY);
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
