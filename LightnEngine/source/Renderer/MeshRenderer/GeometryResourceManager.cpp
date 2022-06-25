#include "GeometryResourceManager.h"
#include <RendererScene/MeshGeometry.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>

namespace ltn {
namespace {
GeometryResourceManager g_geometryResourceManager;
}
void GeometryResourceManager::initialize() {
	CpuScopedPerf scopedPerf("GeometryResourceManager");
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU リソース初期化
	{
		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexPosition);
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_vertexPositionGpuBuffer.initialize(desc);
		_vertexPositionGpuBuffer.setName("VertexPositions");

		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexNormalTangent);
		_vertexNormalTangentGpuBuffer.initialize(desc);
		_vertexNormalTangentGpuBuffer.setName("VertexNormalTangents");

		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexTexCoord);
		_vertexTexcoordGpuBuffer.initialize(desc);
		_vertexTexcoordGpuBuffer.setName("VertexTexcoords");

		desc._sizeInByte = INDEX_COUNT_MAX * sizeof(VertexIndex);
		_indexGpuBuffer.initialize(desc);
		_indexGpuBuffer.setName("VertexIndices");

		desc._sizeInByte = MeshGeometryScene::LOD_MESH_GEOMETRY_CAPACITY * sizeof(gpu::GeometryGlobalOffsetInfo);
		_geometryGlobalOffsetGpuBuffer.initialize(desc);
		_geometryGlobalOffsetGpuBuffer.setName("GeometryGlobalOffsets");
	}


	// 頂点・インデックスアロケーター
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = VERTEX_COUNT_MAX;
		_vertexPositionAllocations.initialize(handleDesc);

		handleDesc._size = INDEX_COUNT_MAX;
		_indexAllocations.initialize(handleDesc);

		_streamedLodRanges = Memory::allocObjects<LodStreamRange>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_geometryAllocationInfos = Memory::allocObjects<GeometryAllocationInfo>(MeshGeometryScene::LOD_MESH_GEOMETRY_CAPACITY);
	}

	// SRV
	{
		DescriptorAllocator* descriptorAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
		_geometryGlobalOffsetSrv = descriptorAllocator->allocate();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._numElements = _geometryGlobalOffsetGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_geometryGlobalOffsetGpuBuffer.getResource(), &desc, _geometryGlobalOffsetSrv._cpuHandle);
	}
}

void GeometryResourceManager::terminate() {
	DescriptorAllocator* descriptorAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
	descriptorAllocator->free(_geometryGlobalOffsetSrv);

	Memory::freeObjects(_streamedLodRanges);
	Memory::freeObjects(_geometryAllocationInfos);
	_vertexPositionGpuBuffer.terminate();
	_vertexNormalTangentGpuBuffer.terminate();
	_vertexTexcoordGpuBuffer.terminate();
	_geometryGlobalOffsetGpuBuffer.terminate();
	_indexGpuBuffer.terminate();
	_vertexPositionAllocations.terminate();
	_indexAllocations.terminate();
}

void GeometryResourceManager::update() {
	MeshGeometryScene* meshScene = MeshGeometryScene::Get();
	const MeshGeometryPool* meshPool = meshScene->getMeshGeometryPool();

	// ジオメトリ情報を追加
	const UpdateInfos<MeshGeometry>* meshCreateInfos = meshScene->getCreateInfos();
	u32 createMeshCount = meshCreateInfos->getUpdateCount();
	auto createMeshes = meshCreateInfos->getObjects();
	for (u32 i = 0; i < createMeshCount; ++i) {
		VramUpdater* vramUpdater = VramUpdater::Get();
		const MeshGeometry* mesh = createMeshes[i];
		loadLodMesh(mesh, 0, mesh->getLodMeshCount());
	}

	// ジオメトリ情報を削除
	const UpdateInfos<MeshGeometry>* meshDestroyInfos = meshScene->getDestroyInfos();
	u32 destroyMeshCount = meshDestroyInfos->getUpdateCount();
	auto destroyMeshes = meshDestroyInfos->getObjects();
	for (u32 i = 0; i < destroyMeshCount; ++i) {
		const MeshGeometry* mesh = destroyMeshes[i];
		u32 meshIndex = meshPool->getMeshGeometryIndex(mesh);

		// ストリーミングされている範囲のジオメトリを削除
		const LodStreamRange& streamRange = _streamedLodRanges[meshIndex];
		for (u16 lodLevel = streamRange._beginLevel; lodLevel < streamRange._endLevel; ++lodLevel) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(lodLevel);
			u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(lodMesh);
			GeometryAllocationInfo& info = _geometryAllocationInfos[lodMeshIndex];
			_vertexPositionAllocations.freeAllocation(info._vertexAllocationInfo);
			_indexAllocations.freeAllocation(info._indexAllocationInfo);
		}
	}
}

rhi::VertexBufferView GeometryResourceManager::getPositionVertexBufferView() const {
	rhi::VertexBufferView vertexBufferView;
	vertexBufferView._bufferLocation = _vertexPositionGpuBuffer.getGpuVirtualAddress();
	vertexBufferView._sizeInBytes = _vertexPositionGpuBuffer.getSizeInByte();
	vertexBufferView._strideInBytes = sizeof(VertexPosition);
	return vertexBufferView;
}

rhi::VertexBufferView GeometryResourceManager::getNormalTangentVertexBufferView() const {
	rhi::VertexBufferView vertexBufferView;
	vertexBufferView._bufferLocation = _vertexNormalTangentGpuBuffer.getGpuVirtualAddress();
	vertexBufferView._sizeInBytes = _vertexNormalTangentGpuBuffer.getSizeInByte();
	vertexBufferView._strideInBytes = sizeof(VertexNormalTangent);
	return vertexBufferView;
}

rhi::VertexBufferView GeometryResourceManager::getTexcoordVertexBufferView() const {
	rhi::VertexBufferView vertexBufferView;
	vertexBufferView._bufferLocation = _vertexTexcoordGpuBuffer.getGpuVirtualAddress();
	vertexBufferView._sizeInBytes = _vertexTexcoordGpuBuffer.getSizeInByte();
	vertexBufferView._strideInBytes = sizeof(VertexTexCoord);
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

// 指定された LOD 範囲のジオメトリ情報を読み取って VRAM アップロード
void GeometryResourceManager::loadLodMesh(const MeshGeometry* mesh, u32 beginLodLevel, u32 endLodLevel) {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MeshGeometryScene* meshScene = MeshGeometryScene::Get();
	const MeshGeometryPool* meshPool = meshScene->getMeshGeometryPool();
	u32 meshIndex = meshPool->getMeshGeometryIndex(mesh);

	u32 beginVertexOffset = 0;
	u32 beginIndexOffset = 0;
	for (u32 lodMeshIndex = 0; lodMeshIndex < beginLodLevel; ++lodMeshIndex) {
		const LodMeshGeometry* lodMesh = mesh->getLodMesh(lodMeshIndex);
		beginVertexOffset += lodMesh->_vertexCount;
		beginIndexOffset += lodMesh->_indexCount;
	}

	u32 endVertexOffset = 0;
	u32 endIndexOffset = 0;
	for (u32 lodMeshIndex = endLodLevel; lodMeshIndex < mesh->getLodMeshCount(); ++lodMeshIndex) {
		const LodMeshGeometry* lodMesh = mesh->getLodMesh(lodMeshIndex);
		endVertexOffset += lodMesh->_vertexCount;
		endIndexOffset += lodMesh->_indexCount;
	}

	LodStreamRange& streamRange = _streamedLodRanges[meshIndex];
	streamRange._beginLevel = Min(streamRange._beginLevel, u16(beginLodLevel));
	streamRange._endLevel = Max(streamRange._endLevel, u16(endLodLevel));

	// .meshg -> .meshgv
	char path[FILE_PATH_COUNT_MAX];
	sprintf_s(path, "%s%s", mesh->getAssetPath(), "v");

	AssetPath assetPath(path);
	assetPath.openFile();

	// ジオメトリオフセット情報
	for (u32 i = beginLodLevel; i < endLodLevel; ++i) {
		const LodMeshGeometry* lodMesh = mesh->getLodMesh(i);
		u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(lodMesh);
		u32 vertexCount = lodMesh->_vertexCount;
		u32 indexCount = lodMesh->_indexCount;
		GeometryAllocationInfo& info = _geometryAllocationInfos[lodMeshIndex];
		info._vertexAllocationInfo = _vertexPositionAllocations.allocation(vertexCount);
		info._indexAllocationInfo = _indexAllocations.allocation(indexCount);

		gpu::GeometryGlobalOffsetInfo* offsetInfo = vramUpdater->enqueueUpdate<gpu::GeometryGlobalOffsetInfo>(&_geometryGlobalOffsetGpuBuffer, lodMeshIndex);
		offsetInfo->_vertexOffset = u32(info._vertexAllocationInfo._offset);
		offsetInfo->_indexOffset = u32(info._indexAllocationInfo._offset);
	}

	// 座標
	{
		assetPath.seekCur(sizeof(VertexPosition) * beginVertexOffset);
		for (u32 i = beginLodLevel; i < endLodLevel; ++i) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(i);
			u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(lodMesh);
			u32 vertexCount = lodMesh->_vertexCount;
			const GeometryAllocationInfo& info = _geometryAllocationInfos[lodMeshIndex];

			u32 globalOffset = u32(info._vertexAllocationInfo._offset);
			u32 localOffset = lodMesh->_vertexOffset;

			u32 beginOffset = sizeof(VertexPosition) * localOffset;
			u32 endOffset = sizeof(VertexPosition) * vertexCount;
			VertexPosition* positions = vramUpdater->enqueueUpdate<VertexPosition>(&_vertexPositionGpuBuffer, globalOffset, vertexCount);
			assetPath.readFile(positions, sizeof(VertexPosition) * vertexCount);
		}
		assetPath.seekCur(sizeof(VertexPosition) * endVertexOffset);
	}

	// 法線・接線
	{
		assetPath.seekCur(sizeof(VertexNormalTangent) * beginVertexOffset);
		for (u32 i = beginLodLevel; i < endLodLevel; ++i) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(i);
			u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(lodMesh);
			u32 vertexCount = lodMesh->_vertexCount;
			const GeometryAllocationInfo& info = _geometryAllocationInfos[lodMeshIndex];

			u32 globalOffset = u32(info._vertexAllocationInfo._offset);
			u32 localOffset = lodMesh->_vertexOffset;

			u32 beginOffset = sizeof(VertexNormalTangent) * localOffset;
			u32 endOffset = sizeof(VertexNormalTangent) * vertexCount;
			VertexNormalTangent* normalTangents = vramUpdater->enqueueUpdate<VertexNormalTangent>(&_vertexNormalTangentGpuBuffer, globalOffset, vertexCount);
			assetPath.readFile(normalTangents, sizeof(VertexNormalTangent) * vertexCount);
		}
		assetPath.seekCur(sizeof(VertexNormalTangent) * endVertexOffset);
	}


	// UV
	{
		assetPath.seekCur(sizeof(VertexTexCoord) * beginVertexOffset);
		for (u32 i = beginLodLevel; i < endLodLevel; ++i) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(i);
			u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(lodMesh);
			u32 vertexCount = lodMesh->_vertexCount;
			const GeometryAllocationInfo& info = _geometryAllocationInfos[lodMeshIndex];

			u32 globalOffset = u32(info._vertexAllocationInfo._offset);
			u32 localOffset = lodMesh->_vertexOffset;

			u32 beginOffset = sizeof(VertexTexCoord) * localOffset;
			u32 endOffset = sizeof(VertexTexCoord) * vertexCount;
			VertexTexCoord* positions = vramUpdater->enqueueUpdate<VertexTexCoord>(&_vertexTexcoordGpuBuffer, globalOffset, vertexCount);
			assetPath.readFile(positions, sizeof(VertexTexCoord) * vertexCount);
		}
		assetPath.seekCur(sizeof(VertexTexCoord) * endVertexOffset);
	}

	// インデックスバッファ
	{
		assetPath.seekCur(sizeof(VertexIndex) * beginVertexOffset);
		for (u32 i = beginLodLevel; i < endLodLevel; ++i) {
			const LodMeshGeometry* lodMesh = mesh->getLodMesh(i);
			u32 lodMeshIndex = meshPool->getLodMeshGeometryIndex(lodMesh);
			u32 indexCount = lodMesh->_indexCount;
			const GeometryAllocationInfo& info = _geometryAllocationInfos[lodMeshIndex];

			u32 globalOffset = u32(info._indexAllocationInfo._offset);
			u32 localOffset = lodMesh->_indexOffset;

			u32 beginOffset = sizeof(VertexIndex) * localOffset;
			u32 endOffset = sizeof(VertexIndex) * indexCount;
			VertexIndex* positions = vramUpdater->enqueueUpdate<VertexIndex>(&_indexGpuBuffer, globalOffset, indexCount);
			assetPath.readFile(positions, sizeof(VertexIndex) * indexCount);
		}
	}

	assetPath.closeFile();
}
}
