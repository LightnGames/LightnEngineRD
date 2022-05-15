#include "GpuMeshResourceManager.h"
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/DeviceManager.h>

namespace ltn {
void GpuMeshResourceManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	GpuBufferDesc desc = {};
	desc._device = device;
	desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexPosition);
	desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	_vertexPositionGpuBuffer.initialize(desc);

	desc._sizeInByte = MeshScene::MESH_COUNT_MAX * sizeof(GpuMesh);
	_meshGpuBuffer.initialize(desc);

	desc._sizeInByte = MeshScene::LOD_MESH_COUNT_MAX * sizeof(GpuLodMesh);
	_lodMeshGpuBuffer.initialize(desc);

	desc._sizeInByte = MeshScene::SUB_MESH_COUNT_MAX * sizeof(GpuSubMesh);
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
	MeshObserver* meshObserver = meshScene->getMeshObserver();

	// 新規作成されたメッシュをGPUに追加
	u32 updatedMeshCount = meshObserver->getCreatedMeshCount();
	Mesh** updatedMeshes = meshObserver->getCreatedMeshes();
	for (u32 i = 0; i < updatedMeshCount; ++i) {

	}

	// 削除されたメッシュをGPUから削除
	u32 deletedMeshCount = meshObserver->getDeletedMeshCount();
	Mesh** deletedMeshes = meshObserver->getDeletedMeshes();
	for (u32 i = 0; i < deletedMeshCount; ++i) {

	}
}
}
