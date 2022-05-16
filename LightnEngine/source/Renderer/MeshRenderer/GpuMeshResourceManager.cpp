#include "GpuMeshResourceManager.h"
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/DeviceManager.h>

namespace ltn {
namespace {
GpuMeshResourceManager g_gpuMeshResourceManager;
}

void GpuMeshResourceManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	GpuBufferDesc desc = {};
	desc._device = device;
	desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexPosition);
	desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	_vertexPositionGpuBuffer.initialize(desc);

	desc._sizeInByte = MeshScene::MESH_COUNT_MAX * sizeof(gpu::Mesh);
	_meshGpuBuffer.initialize(desc);

	desc._sizeInByte = MeshScene::LOD_MESH_COUNT_MAX * sizeof(gpu::LodMesh);
	_lodMeshGpuBuffer.initialize(desc);

	desc._sizeInByte = MeshScene::SUB_MESH_COUNT_MAX * sizeof(gpu::SubMesh);
	_subMeshGpuBuffer.initialize(desc);
}

void GpuMeshResourceManager::terminate() {
	_vertexPositionGpuBuffer.terminate();
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
		Mesh* mesh = createdMeshes[i];
		u32 vertexCount = mesh->_vertexCount;
	}

	// 削除されたメッシュをGPUから削除
	u32 deletedMeshCount = meshUpdateInfos->getDeletedMeshCount();
	Mesh** deletedMeshes = meshUpdateInfos->getDeletedMeshes();
	for (u32 i = 0; i < deletedMeshCount; ++i) {

	}
}

GpuMeshResourceManager* GpuMeshResourceManager::Get() {
	return &g_gpuMeshResourceManager;
}
}
