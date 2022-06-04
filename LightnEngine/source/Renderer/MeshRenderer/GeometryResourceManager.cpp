#include "GeometryResourceManager.h"
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Core/Memory.h>

namespace ltn {
namespace {
GeometryResourceManager g_geometryResourceManager;
}
void GeometryResourceManager::initialize() {
	// GPU ���\�[�X������
	{
		rhi::Device* device = DeviceManager::Get()->getDevice();
		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexPosition);
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_vertexPositionGpuBuffer.initialize(desc);
		_vertexPositionGpuBuffer.setName("VertexPositions");

		desc._sizeInByte = INDEX_COUNT_MAX * sizeof(VertexIndex);
		_indexGpuBuffer.initialize(desc);
		_indexGpuBuffer.setName("VertexIndices");
	}


	// ���_�E�C���f�b�N�X�A���P�[�^�[
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = VERTEX_COUNT_MAX;
		_vertexPositionAllocations.initialize(handleDesc);

		handleDesc._size = INDEX_COUNT_MAX;
		_indexAllocations.initialize(handleDesc);

		_meshInfos = Memory::allocObjects<MeshGeometryInfo>(MeshScene::MESH_CAPACITY);
	}
}

void GeometryResourceManager::terminate() {
	Memory::freeObjects(_meshInfos);
	_vertexPositionGpuBuffer.terminate();
	_indexGpuBuffer.terminate();
	_vertexPositionAllocations.terminate();
	_indexAllocations.terminate();
}

void GeometryResourceManager::update() {
	MeshScene* meshScene = MeshScene::Get();
	const MeshPool* meshPool = meshScene->getMeshPool();

	const UpdateInfos<Mesh>* meshCreateInfos = meshScene->geCreateInfos();
	u32 createMeshCount = meshCreateInfos->getUpdateCount();
	auto createMeshes = meshCreateInfos->getObjects();
	for (u32 i = 0; i < createMeshCount; ++i) {
		VramUpdater* vramUpdater = VramUpdater::Get();
		const Mesh* mesh = createMeshes[i];
		u32 meshIndex = meshPool->getMeshIndex(mesh);
		MeshGeometryInfo& info = _meshInfos[meshIndex];
		info._vertexAllocationInfo = _vertexPositionAllocations.allocation(mesh->_vertexCount);
		info._indexAllocationInfo = _indexAllocations.allocation(mesh->_indexCount);
	}

	const UpdateInfos<Mesh>* meshDestroyInfos = meshScene->geCreateInfos();
	u32 destroyMeshCount = meshDestroyInfos->getUpdateCount();
	auto destroyMeshes = meshDestroyInfos->getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		const Mesh* mesh = destroyMeshes[i];
		u32 meshIndex = meshPool->getMeshIndex(mesh);
		MeshGeometryInfo& info = _meshInfos[meshIndex];
		_vertexPositionAllocations.freeAllocation(info._vertexAllocationInfo);
		_indexAllocations.freeAllocation(info._indexAllocationInfo);
	}
}

rhi::VertexBufferView GeometryResourceManager::getPositionVertexBufferView() const {
	rhi::VertexBufferView vertexBufferView;
	vertexBufferView._bufferLocation = _vertexPositionGpuBuffer.getGpuVirtualAddress();
	vertexBufferView._sizeInBytes = _vertexPositionGpuBuffer.getSizeInByte();
	vertexBufferView._strideInBytes = sizeof(VertexPosition);
	return vertexBufferView;
}

rhi::IndexBufferView GeometryResourceManager::getIndexBufferView() const {
	rhi::IndexBufferView indexBufferView;
	indexBufferView._bufferLocation = _indexGpuBuffer.getGpuVirtualAddress();
	indexBufferView._sizeInBytes = _indexGpuBuffer.getSizeInByte();
	indexBufferView._format = rhi::FORMAT_R32_UINT;
	return indexBufferView;
}

GeometryResourceManager* GeometryResourceManager::Get() {
	return &g_geometryResourceManager;
}
}
