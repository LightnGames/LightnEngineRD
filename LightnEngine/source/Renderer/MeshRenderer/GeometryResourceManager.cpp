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
	// GPU リソース初期化
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


	// 頂点・インデックスアロケーター
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

	const UpdateInfos<Mesh>* meshCreateInfos = meshScene->getCreateInfos();
	u32 createMeshCount = meshCreateInfos->getUpdateCount();
	auto createMeshes = meshCreateInfos->getObjects();
	for (u32 i = 0; i < createMeshCount; ++i) {
		VramUpdater* vramUpdater = VramUpdater::Get();
		const Mesh* mesh = createMeshes[i];
		loadLodMesh(mesh, 0, 1);
	}

	const UpdateInfos<Mesh>* meshDestroyInfos = meshScene->getDestroyInfos();
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

void GeometryResourceManager::loadLodMesh(const Mesh* mesh, u32 beginLodLevel, u32 endLodLevel) {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MeshScene* meshScene = MeshScene::Get();
	const MeshPool* meshPool = meshScene->getMeshPool();
	u32 meshIndex = meshPool->getMeshIndex(mesh);
	MeshGeometryInfo& info = _meshInfos[meshIndex];
	info._vertexAllocationInfo = _vertexPositionAllocations.allocation(mesh->getVertexCount());
	info._indexAllocationInfo = _indexAllocations.allocation(mesh->getIndexCount());

	// .mesh -> .meshg
	char path[FILE_PATH_COUNT_MAX];
	sprintf_s(path, "%s%s", mesh->getAssetPath(), "g");

	AssetPath assetPath(path);
	assetPath.openFile();

	// 座標
	{
		u32 count = mesh->getVertexCount();
		u32 offset = u32(info._vertexAllocationInfo._offset);
		VertexPosition* positions = vramUpdater->enqueueUpdate<VertexPosition>(&_vertexPositionGpuBuffer, offset, count);
		assetPath.readFile(positions, sizeof(VertexPosition) * count);

		// 法線・UVダミー読み取り
		assetPath.seekCur(sizeof(VertexNormalTangent) * count);
		assetPath.seekCur(sizeof(VertexTexCoords) * count);

		//u32 endOffset = sizeof(VertexPosition) * endVertexOffset;
		//u32 srcOffset = sizeof(VertexPosition) * beginVertexOffset;
		//u32 dstOffset = sizeof(VertexPosition) * vertexBinaryIndex;
		//VertexPosition* vertexPtr = vramUpdater->enqueueUpdate<VertexPosition>(&_positionVertexBuffer, dstOffset, vertexCount);
		//fseek(fin, srcOffset, SEEK_CUR);
		//fread_s(vertices.data(), sizeof(VertexPosition) * vertexCount, sizeof(VertexPosition), vertexCount, fin);
		//fseek(fin, endOffset, SEEK_CUR);
		//memcpy(vertexPtr, vertices.data(), sizeof(VertexPosition) * vertices.size());
	}

	// インデックスバッファ
	{
		u32 count = mesh->getIndexCount();
		u32 offset = u32(info._indexAllocationInfo._offset);
		VertexIndex* indices = vramUpdater->enqueueUpdate<VertexIndex>(&_indexGpuBuffer, offset, count);
		assetPath.readFile(indices, sizeof(VertexIndex) * count);

		//u32 endOffset = sizeof(u32) * endClassicIndexOffset;
		//u32 srcOffset = sizeof(u32) * beginClassicIndexOffset;
		//u32 dstOffset = sizeof(u32) * classicIndex;
		//u32* classicIndexPtr = vramUpdater->enqueueUpdate<u32>(&_classicIndexBuffer, dstOffset, classicIndexCount);
		//fseek(fin, srcOffset, SEEK_CUR);
		//fread_s(triangles.data(), sizeof(u32) * classicIndexCount, sizeof(u32), classicIndexCount, fin);
		//fseek(fin, endOffset, SEEK_CUR);
		//memcpy(classicIndexPtr, triangles.data(), sizeof(u32) * classicIndexCount);
	}

	assetPath.closeFile();
}
}
