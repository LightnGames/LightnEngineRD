#include <MeshRenderer/impl/MeshResourceManager.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/QueryHeapSystem.h>
#include <DebugRenderer/DebugRendererSystem.h>
#include <GfxCore/impl/ViewSystemImpl.h>//TODO: SDF　テストが終わったら消す
#include "../third_party/sdf_gen/makelevelset3.h"

void MeshResourceManager::initialize() {
	Device* device = GraphicsSystemImpl::Get()->getDevice();

	// buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexPosition);
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._device = device;
		_positionVertexBuffer.initialize(desc);
		_positionVertexBuffer.setDebugName("Position Vertex");

		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexNormalTangent);
		_normalTangentVertexBuffer.initialize(desc);
		_normalTangentVertexBuffer.setDebugName("Normal Tangent Vertex");

		desc._sizeInByte = VERTEX_COUNT_MAX * sizeof(VertexTexcoord);
		_texcoordVertexBuffer.initialize(desc);
		_texcoordVertexBuffer.setDebugName("Texcoord Vertex");

		desc._sizeInByte = INDEX_COUNT_MAX * sizeof(u32);
		_vertexIndexBuffer.initialize(desc);
		_vertexIndexBuffer.setDebugName("Index Vertex");

		desc._sizeInByte = INDEX_COUNT_MAX * sizeof(u32);
		_primitiveBuffer.initialize(desc);
		_primitiveBuffer.setDebugName("Primitive");

#if ENABLE_CLASSIC_VERTEX
		desc._sizeInByte = INDEX_COUNT_MAX * sizeof(u32);
		_classicIndexBuffer.initialize(desc);
		_classicIndexBuffer.setDebugName("Classic Index");
#endif
		desc._sizeInByte = SUB_MESH_COUNT_MAX * sizeof(gpu::SubMeshDrawInfo);
		_subMeshDrawInfoBuffer.initialize(desc);
		_subMeshDrawInfoBuffer.setDebugName("SubMesh Draw Info");

		desc._sizeInByte = MESH_COUNT_MAX * sizeof(gpu::Mesh);
		_meshBuffer.initialize(desc);
		_meshBuffer.setDebugName("Mesh");

		desc._sizeInByte = LOD_MESH_COUNT_MAX * sizeof(gpu::LodMesh);
		_lodMeshBuffer.initialize(desc);
		_lodMeshBuffer.setDebugName("LodMesh");

		desc._sizeInByte = SUB_MESH_COUNT_MAX * sizeof(gpu::SubMesh);
		_subMeshBuffer.initialize(desc);
		_subMeshBuffer.setDebugName("SubMesh");

		desc._sizeInByte = MESHLET_COUNT_MAX * sizeof(gpu::Meshlet);
		_meshletBuffer.initialize(desc);
		_meshletBuffer.setDebugName("Meshlet");

		desc._sizeInByte = MESHLET_COUNT_MAX * sizeof(gpu::MeshletPrimitiveInfo);
		_meshletPrimitiveInfoBuffer.initialize(desc);
		_meshletPrimitiveInfoBuffer.setDebugName("Meshlet Primitive Info");

		desc._sizeInByte = MESH_COUNT_MAX * sizeof(gpu::MeshBounds);
		_meshBoundsSizeBuffer.initialize(desc);
		_meshBoundsSizeBuffer.setDebugName("Mesh Bounds Size");
	}

	// Mesh lod level feedback buffers
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._sizeInByte = MESH_COUNT_MAX * sizeof(u32);
		desc._initialState = RESOURCE_STATE_UNORDERED_ACCESS;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		_meshLodLevelFeedbackBuffer.initialize(desc);
		_meshLodLevelFeedbackBuffer.setDebugName("Mesh Lod Level Feedback");

		desc._sizeInByte = MESH_COUNT_MAX * sizeof(u32) * BACK_BUFFER_COUNT;
		desc._initialState = RESOURCE_STATE_COPY_DEST;
		desc._heapType = HEAP_TYPE_READBACK;
		desc._flags = RESOURCE_FLAG_NONE;
		_meshLodLevelFeedbackReadbackBuffer.initialize(desc);
		_meshLodLevelFeedbackReadbackBuffer.setDebugName("Mesh Lod Level Feedback Readback");
	}

	_meshes.initialize(MESH_COUNT_MAX);
	_lodMeshes.initialize(LOD_MESH_COUNT_MAX);
	_subMeshes.initialize(SUB_MESH_COUNT_MAX);
	_meshlets.initialize(MESHLET_COUNT_MAX);

	_vertexPositionBinaryHeaders.initialize(VERTEX_COUNT_MAX);
	_indexBinaryHeaders.initialize(INDEX_COUNT_MAX);
	_primitiveBinaryHeaders.initialize(PRIMITIVE_COUNT_MAX);
#if ENABLE_CLASSIC_VERTEX
	_classicIndexBinaryHeaders.initialize(INDEX_COUNT_MAX);
#endif

	// descriptors
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
		_meshSrv = allocator->allocateDescriptors(5);
		_vertexSrv = allocator->allocateDescriptors(5);
		_subMeshDrawInfoSrv = allocator->allocateDescriptors(1);
		_meshLodLevelFeedbackUav = allocator->allocateDescriptors(1);
		_meshLodLevelFeedbackCpuUav = cpuAllocator->allocateDescriptors(1);
		_meshBoundsSizeSrv = allocator->allocateDescriptors(1);
		_classicIndexSrv = allocator->allocateDescriptors(1);
		u64 incrimentSize = static_cast<u64>(allocator->getIncrimentSize());

		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = BUFFER_SRV_FLAG_NONE;

		// mesh
		{
			CpuDescriptorHandle handle = _meshSrv._cpuHandle;

			desc._buffer._numElements = MESH_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::Mesh);
			device->createShaderResourceView(_meshBuffer.getResource(), &desc, handle + incrimentSize * 0);

			desc._buffer._numElements = LOD_MESH_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::LodMesh);
			device->createShaderResourceView(_lodMeshBuffer.getResource(), &desc, handle + incrimentSize * 1);

			desc._buffer._numElements = SUB_MESH_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::SubMesh);
			device->createShaderResourceView(_subMeshBuffer.getResource(), &desc, handle + incrimentSize * 2);

			desc._buffer._numElements = MESHLET_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::Meshlet);
			device->createShaderResourceView(_meshletBuffer.getResource(), &desc, handle + incrimentSize * 3);

			desc._buffer._numElements = MESHLET_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::MeshletPrimitiveInfo);
			device->createShaderResourceView(_meshletPrimitiveInfoBuffer.getResource(), &desc, handle + incrimentSize * 4);

			desc._buffer._numElements = SUB_MESH_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::SubMeshDrawInfo);
			device->createShaderResourceView(_subMeshDrawInfoBuffer.getResource(), &desc, _subMeshDrawInfoSrv._cpuHandle);

			desc._buffer._numElements = MESH_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::MeshBounds);
			device->createShaderResourceView(_meshBoundsSizeBuffer.getResource(), &desc, _meshBoundsSizeSrv._cpuHandle);
		}

		// vertex
		{
			CpuDescriptorHandle handle = _vertexSrv._cpuHandle;

			desc._buffer._numElements = VERTEX_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(u32);
			device->createShaderResourceView(_vertexIndexBuffer.getResource(), &desc, handle + incrimentSize * 0);
			device->createShaderResourceView(_primitiveBuffer.getResource(), &desc, handle + incrimentSize * 1);

			desc._buffer._structureByteStride = sizeof(VertexPosition);
			device->createShaderResourceView(_positionVertexBuffer.getResource(), &desc, handle + incrimentSize * 2);

			desc._buffer._structureByteStride = sizeof(VertexNormalTangent);
			device->createShaderResourceView(_normalTangentVertexBuffer.getResource(), &desc, handle + incrimentSize * 3);

			desc._buffer._structureByteStride = sizeof(VertexTexcoord);
			device->createShaderResourceView(_texcoordVertexBuffer.getResource(), &desc, handle + incrimentSize * 4);

			desc._buffer._numElements = INDEX_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(u32);
			device->createShaderResourceView(_classicIndexBuffer.getResource(), &desc, _classicIndexSrv._cpuHandle);
		}
	}

	{
		UnorderedAccessViewDesc desc = {};
		desc._format = FORMAT_R32_TYPELESS;
		desc._viewDimension = UAV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = MESH_COUNT_MAX;
		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		device->createUnorderedAccessView(_meshLodLevelFeedbackBuffer.getResource(), nullptr, &desc, _meshLodLevelFeedbackUav._cpuHandle);
		device->createUnorderedAccessView(_meshLodLevelFeedbackBuffer.getResource(), nullptr, &desc, _meshLodLevelFeedbackCpuUav._cpuHandle);
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_meshSdfSrv = allocator->allocateDescriptors(MESH_COUNT_MAX);
		_meshSdfUav = allocator->allocateDescriptors(MESH_COUNT_MAX);
	}

	MeshDesc desc = {};
	desc._filePath = "common/box.mesh";
	_defaultCube = createMesh(desc);

	_sdfGenerator.initialize();
}

void MeshResourceManager::terminate() {
	_sdfGenerator.terminate();

	_positionVertexBuffer.terminate();
	_normalTangentVertexBuffer.terminate();
	_texcoordVertexBuffer.terminate();
	_vertexIndexBuffer.terminate();
	_primitiveBuffer.terminate();
	_meshBuffer.terminate();
	_lodMeshBuffer.terminate();
	_subMeshBuffer.terminate();
	_meshletBuffer.terminate();
	_meshletPrimitiveInfoBuffer.terminate();
	_subMeshDrawInfoBuffer.terminate();
	_meshLodLevelFeedbackBuffer.terminate();
	_meshLodLevelFeedbackReadbackBuffer.terminate();
	_meshBoundsSizeBuffer.terminate();

#if ENABLE_CLASSIC_VERTEX
	_classicIndexBuffer.terminate();
	_classicIndexBinaryHeaders.terminate();
#endif

	LTN_ASSERT(_meshes.getInstanceCount() == 0);
	LTN_ASSERT(_lodMeshes.getInstanceCount() == 0);
	LTN_ASSERT(_subMeshes.getInstanceCount() == 0);

	_meshes.terminate();
	_lodMeshes.terminate();
	_subMeshes.terminate();
	_meshlets.terminate();

	_vertexPositionBinaryHeaders.terminate();
	_indexBinaryHeaders.terminate();
	_primitiveBinaryHeaders.terminate();

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_meshSrv);
	allocator->discardDescriptor(_vertexSrv);
	allocator->discardDescriptor(_subMeshDrawInfoSrv);
	allocator->discardDescriptor(_meshLodLevelFeedbackUav);
	allocator->discardDescriptor(_meshSdfSrv);
	allocator->discardDescriptor(_meshSdfUav);
	allocator->discardDescriptor(_meshBoundsSizeSrv);
	allocator->discardDescriptor(_classicIndexSrv);

	DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
	cpuAllocator->discardDescriptor(_meshLodLevelFeedbackCpuUav);
}

void MeshResourceManager::update() {
	u32 meshCount = _meshes.getResarveCount();
	for (u32 meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		if (_assetStateFlags[meshIndex] == ASSET_STATE_REQUEST_LOAD) {
			loadMesh(meshIndex);
		}
	}

	// 要求 Lod Level をリードバック
	{
		u32 frameIndex = GraphicsSystemImpl::Get()->getFrameIndex();
		u64 startOffset = static_cast<u64>(frameIndex) * MESH_COUNT_MAX;
		MemoryRange range(startOffset, MESH_COUNT_MAX);
		u32* mapPtr = _meshLodLevelFeedbackReadbackBuffer.map<u32>(&range);
		memcpy(_meshLodLevelFeedbacks, mapPtr, sizeof(u32) * MESH_COUNT_MAX);
		_meshLodLevelFeedbackReadbackBuffer.unmap();
	}

	for (u32 meshIndex = 0; meshIndex < _meshes.getResarveCount(); ++meshIndex) {
		if (_assetStateFlags[meshIndex] != ASSET_STATE_ENABLE) {
			continue;
		}

		u32 feedbackLodLevel = _meshLodLevelFeedbacks[meshIndex];
		if (feedbackLodLevel == gpu::INVALID_INDEX) {
			continue;
		}

		gpu::Mesh& mesh = _meshes[meshIndex];
		if (feedbackLodLevel == mesh._streamedLodLevel) {
			continue;
		}

		if (feedbackLodLevel < mesh._streamedLodLevel) {
			loadLodMeshes(meshIndex, feedbackLodLevel, mesh._streamedLodLevel);
		}

		if (feedbackLodLevel > mesh._streamedLodLevel) {
			deleteLodMeshes(meshIndex, mesh._streamedLodLevel, feedbackLodLevel);
		}

		mesh._streamedLodLevel = feedbackLodLevel;

		VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
		gpu::Mesh* meshes = vramUpdater->enqueueUpdate<gpu::Mesh>(&_meshBuffer, sizeof(gpu::Mesh) * meshIndex);
		memcpy(meshes, &_meshes[meshIndex], sizeof(gpu::Mesh));
	}

	DebugGui::Start("Mesh Lod Level Feedback");
	for (u32 i = 0; i < _meshes.getResarveCount(); ++i) {
		DebugGui::Text("[%2d] %-70s", _meshLodLevelFeedbacks[i], _debugMeshInfo[i]._filePath);
	}
	DebugGui::End();

	_sdfGenerator.update();
}

void MeshResourceManager::processDeletion() {
	u32 meshCount = _meshes.getResarveCount();
	for (u32 meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		if (_meshStateFlags[meshIndex] & MESH_FLAG_STATE_REQUEST_DESTROY) {
			deleteMesh(meshIndex);
		}
	}
}

void MeshResourceManager::terminateDefaultResources() {
	_defaultCube->requestToDelete();
}

void MeshResourceManager::drawDebugGui() {
	u32 meshCount = _meshes.getResarveCount();
	DebugGui::Text("Total:%3d", meshCount);
	DebugGui::Columns(2, "tree", true);
	constexpr f32 MESH_NAME_WIDTH = 320;
	DebugGui::SetColumnWidth(0, MESH_NAME_WIDTH);
	for (u32 x = 0; x < meshCount; x++) {
		if (_fileHashes[x] == 0) {
			DebugGui::TextDisabled("Disabled");
			DebugGui::NextColumn();
			DebugGui::TextDisabled("Disabled");
			DebugGui::NextColumn();
			continue;
		}
		const MeshInfo& meshInfo = _meshInfos[x];
		const SubMeshInfo* subMeshInfos = &_subMeshInfos[meshInfo._subMeshStartIndex];
		const DebugMeshInfo& debugMeshInfo = _debugMeshInfo[x];
		bool open1 = DebugGui::TreeNode(static_cast<s32>(x), "%-3d: %-20s", x, debugMeshInfo._filePath);
		Color4 lodCountTextColor = open1 ? Color4::GREEN : Color4::WHITE;
		DebugGui::NextColumn();
		DebugGui::TextColored(lodCountTextColor, "Lod Count:%-2d", meshInfo._totalLodMeshCount);
		DebugGui::NextColumn();
		if (!open1) {
			continue;
		}

		// mesh info
		DebugGui::Separator();
		u32 subMeshIndex = 0;
		for (u32 y = 0; y < meshInfo._totalLodMeshCount; y++) {
			bool open2 = DebugGui::TreeNode(static_cast<s32>(y), "Lod %2d", y);
			u32 subMeshCount = _lodMeshes[meshInfo._lodMeshStartIndex + y]._subMeshCount;
			Color4 subMeshCountTextColor = open2 ? Color4::GREEN : Color4::WHITE;
			DebugGui::NextColumn();
			DebugGui::TextColored(subMeshCountTextColor, "Sub Mesh Count:%2d", subMeshCount);
			DebugGui::NextColumn();
			if (!open2) {
				continue;
			}

			DebugGui::Separator();

			constexpr char lodFormat[] = "%-14s:%-7d";
			DebugGui::Text(lodFormat, "Vertex Count", meshInfo._vertexCount);
			DebugGui::Text(lodFormat, "Index Count", meshInfo._indexCount);

			constexpr char boundsFormat[] = "%-14s:[%5.1f][%5.1f][%5.1f]";
			DebugGui::Text(boundsFormat, "Bounds Min", meshInfo._boundsMin._x, meshInfo._boundsMin._y, meshInfo._boundsMin._z);
			DebugGui::Text(boundsFormat, "Bounds Max", meshInfo._boundsMax._x, meshInfo._boundsMax._y, meshInfo._boundsMax._z);

			for (u32 z = 0; z < subMeshCount; z++) {
				bool open3 = DebugGui::TreeNode(static_cast<s32>(z), "Sub Mesh %2d", z);
				Color4 meshletCountTextColor = open3 ? Color4::GREEN : Color4::WHITE;
				u32 subMeshLocalIndex = subMeshIndex + z;
				u32 meshletCount = subMeshInfos[z]._meshletCount;
				DebugGui::NextColumn();
				DebugGui::TextColored(meshletCountTextColor, "Meshlet Count:%2d", meshletCount);
				DebugGui::NextColumn();

				if (!open3) {
					continue;
				}

				DebugGui::Separator();
				u32 materialSlotIndex = _subMeshes[meshInfo._subMeshStartIndex + subMeshLocalIndex]._materialSlotIndex;
				DebugGui::Text("%10s:%2d", "Slot Index", materialSlotIndex);

				for (u32 w = 0; w < meshletCount; w++) {
					DebugGui::Text("Meshlet %2d", w);
				}

				//メッシュレット情報
				DebugGui::TreePop();
			}
			subMeshIndex += subMeshCount;
			DebugGui::TreePop();
		}

		DebugGui::TreePop();
	}
	DebugGui::Columns(1);

	DebugGui::EndTabItem();
}

void MeshResourceManager::loadMesh(u32 meshIndex) {
	LTN_ASSERT(_assetStateFlags[meshIndex] == ASSET_STATE_REQUEST_LOAD);

	gpu::Mesh& mesh = _meshes[meshIndex];
	mesh._stateFlags = gpu::MESH_STATE_LOADED;

	// もっとも粗いLODレベルのみロードする
	loadLodMeshes(meshIndex, mesh._streamedLodLevel, mesh._streamedLodLevel + 1);

	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	gpu::Mesh* meshes = vramUpdater->enqueueUpdate<gpu::Mesh>(&_meshBuffer, sizeof(gpu::Mesh) * meshIndex);
	memcpy(meshes, &_meshes[meshIndex], sizeof(gpu::Mesh));

	_assetStateFlags[meshIndex] = ASSET_STATE_ENABLE;
}

MeshImpl* MeshResourceManager::allocateMesh(const MeshDesc& desc) {
	u32 meshIndex = _meshes.request();
	MeshImpl* meshImpl = &_meshInstances[meshIndex];

	DebugMeshInfo& debugMeshInfo = _debugMeshInfo[meshIndex];
	memcpy(debugMeshInfo._filePath, desc._filePath, StrLength(desc._filePath));

	u64 fileHash = StrHash(desc._filePath);
	debugMeshInfo._filePathHash = fileHash;
	_fileHashes[meshIndex] = fileHash;

	// アセット実パスに変換
	char meshFilePath[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(meshFilePath, "%s/%s", RESOURCE_FOLDER_PATH, desc._filePath);

	Asset& asset = _meshAssets[meshIndex];
	asset.openFile(meshFilePath);
	asset.setAssetStateFlags(&_assetStateFlags[meshIndex]);
	_assetStateFlags[meshIndex] = ASSET_STATE_ALLOCATED;

	constexpr u32 LOD_PER_MESH_COUNT_MAX = 8;
	constexpr u32 SUBMESH_PER_MESH_COUNT_MAX = 64;
	constexpr u32 MATERIAL_SLOT_COUNT_MAX = 12;
	constexpr u32 MESHLET_INDEX_COUNT = 126;

	// 以下はメッシュエクスポーターと同一の定義にする
	struct SubMeshInfoE {
		u32 _materialSlotIndex = 0;
		u32 _meshletCount = 0;
		u32 _meshletStartIndex = 0;
		u32 _triangleStripIndexCount = 0;
		u32 _triangleStripIndexOffset = 0;
	};

	struct LodInfo {
		u32 _vertexOffset = 0;
		u32 _vertexIndexOffset = 0;
		u32 _primitiveOffset = 0;
		u32 _indexOffset = 0;
		u32 _subMeshOffset = 0;
		u32 _subMeshCount = 0;
		u32 _vertexCount = 0;
		u32 _vertexIndexCount = 0;
		u32 _primitiveCount = 0;
	};
	// =========================================

	// IO読み取り初期化
	FILE* fin = asset.getFile();

	u32 totalLodMeshCount = 0;
	u32 totalSubMeshCount = 0;
	u32 totalMeshletCount = 0;
	u32 totalVertexCount = 0;
	u32 totalVertexIndexCount = 0;
	u32 totalPrimitiveCount = 0;
	u32 materialSlotCount = 0;
	u32 classicIndexCount = 0;
	Vector3 boundsMin;
	Vector3 boundsMax;
	SubMeshInfoE inputSubMeshInfos[SUBMESH_PER_MESH_COUNT_MAX] = {};
	LodInfo lodInfos[LOD_PER_MESH_COUNT_MAX] = {};
	u64 materialNameHashes[MATERIAL_SLOT_COUNT_MAX] = {};

	// load header
	{
		fread_s(&materialSlotCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&totalSubMeshCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&totalLodMeshCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&totalMeshletCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&totalVertexCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&totalVertexIndexCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&totalPrimitiveCount, sizeof(u32), sizeof(u32), 1, fin);
		fread_s(&boundsMin, sizeof(Vector3), sizeof(Vector3), 1, fin);
		fread_s(&boundsMax, sizeof(Vector3), sizeof(Vector3), 1, fin);
		fread_s(&classicIndexCount, sizeof(u32), sizeof(u32), 1, fin);
	}

	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();

	u32 lodMeshStartIndex = _lodMeshes.request(totalLodMeshCount);
	u32 subMeshStartIndex = _subMeshes.request(totalSubMeshCount);
	u32 meshletStartIndex = _meshlets.request(totalMeshletCount);
	LTN_ASSERT(lodMeshStartIndex != MultiDynamicQueueBlockManager::INVILD_BLOCK_INDEX);
	LTN_ASSERT(subMeshStartIndex != MultiDynamicQueueBlockManager::INVILD_BLOCK_INDEX);
	LTN_ASSERT(meshletStartIndex != MultiDynamicQueueBlockManager::INVILD_BLOCK_INDEX);


	{
		fread_s(materialNameHashes, sizeof(u64) * MATERIAL_SLOT_COUNT_MAX, sizeof(u64), materialSlotCount, fin);
		fread_s(inputSubMeshInfos, sizeof(SubMeshInfoE) * SUBMESH_PER_MESH_COUNT_MAX, sizeof(SubMeshInfoE), totalSubMeshCount, fin);
		fread_s(lodInfos, sizeof(LodInfo) * LOD_PER_MESH_COUNT_MAX, sizeof(LodInfo), totalLodMeshCount, fin);
	}

	Vec3f min_box(boundsMin._x, boundsMin._y, boundsMin._z);
	Vec3f max_box(boundsMax._x, boundsMax._y, boundsMax._z);

	f32 dx = 0.2f;
	f32 padding = 2.0f;
	Vec3f unit(1, 1, 1);
	min_box -= padding * dx * unit;
	max_box += padding * dx * unit;
	Vec3ui ceiledSize = Vec3ui((max_box - min_box) / dx);
	Vec3i ceiledCenter = Vec3i((max_box + min_box) / 2.0f / dx);
	Vector3 center(static_cast<f32>(ceiledCenter[0]), static_cast<f32>(ceiledCenter[1]), static_cast<f32>(ceiledCenter[2]));
	Vector3 halfSize = Vector3(static_cast<f32>(ceiledSize[0]), static_cast<f32>(ceiledSize[1]), static_cast<f32>(ceiledSize[2])) / 2.0f;
	LTN_ASSERT(ceiledSize[0] <= 128);
	LTN_ASSERT(ceiledSize[1] <= 128);
	LTN_ASSERT(ceiledSize[2] <= 128);

	MeshInfo& meshInfo = _meshInfos[meshIndex];
	meshInfo._meshIndex = meshIndex;
	meshInfo._vertexCount = totalVertexCount;
	meshInfo._indexCount = totalVertexIndexCount;
	meshInfo._primitiveCount = totalPrimitiveCount;
	meshInfo._totalLodMeshCount = totalLodMeshCount;
	meshInfo._totalSubMeshCount = totalSubMeshCount;
	meshInfo._totalMeshletCount = totalMeshletCount;
	meshInfo._materialSlotCount = materialSlotCount;
	meshInfo._lodMeshStartIndex = lodMeshStartIndex;
	meshInfo._subMeshStartIndex = subMeshStartIndex;
	meshInfo._meshletStartIndex = meshletStartIndex;
	meshInfo._boundsMin = boundsMin;
	meshInfo._boundsMax = boundsMax;
	meshInfo._sdfBoundsMin = (center - halfSize) * dx;
	meshInfo._sdfBundsMax = (center + halfSize) * dx;
	meshInfo._sdfResolution[0] = ceiledSize[0];
	meshInfo._sdfResolution[1] = ceiledSize[1];
	meshInfo._sdfResolution[2] = ceiledSize[2];
	meshInfo._sdfGridSize = dx;
	meshInfo._classicIndexCount = classicIndexCount;

	for (u32 lodMeshLocalIndex = 0; lodMeshLocalIndex < totalLodMeshCount; ++lodMeshLocalIndex) {
		const LodInfo& info = lodInfos[lodMeshLocalIndex];

		meshInfo._subMeshCounts[lodMeshLocalIndex] = info._subMeshCount;
		meshInfo._subMeshOffsets[lodMeshLocalIndex] = info._subMeshOffset;
		meshInfo._vertexCounts[lodMeshLocalIndex] = info._vertexCount;
		meshInfo._vertexIndexCounts[lodMeshLocalIndex] = info._vertexIndexCount;
		meshInfo._primitiveCounts[lodMeshLocalIndex] = info._primitiveCount;

		u32 classicIndexCount = 0;
		u32 meshletCount = 0;
		for (u32 subMeshIndex = 0; subMeshIndex < info._subMeshCount; ++subMeshIndex) {
			const SubMeshInfoE& inputInfo = inputSubMeshInfos[info._subMeshOffset + subMeshIndex];
			classicIndexCount += inputInfo._triangleStripIndexCount;
			meshletCount += inputInfo._meshletCount;
		}
		meshInfo._classicIndexCounts[lodMeshLocalIndex] = classicIndexCount;
		meshInfo._meshletCounts[lodMeshLocalIndex] = meshletCount;
		meshInfo._meshletOffsets[lodMeshLocalIndex] = inputSubMeshInfos[info._subMeshOffset]._meshletStartIndex;
	}

	for (u32 subMeshIndex = 0; subMeshIndex < totalSubMeshCount; ++subMeshIndex) {
		const SubMeshInfoE& inputInfo = inputSubMeshInfos[subMeshIndex];
		SubMeshInfo& info = _subMeshInfos[subMeshStartIndex + subMeshIndex];
		info._meshletOffset = inputInfo._meshletStartIndex;
		info._classicIndexCount = inputInfo._triangleStripIndexCount;
		info._classiciIndexOffset = inputInfo._triangleStripIndexOffset;
		info._meshletCount = inputInfo._meshletCount;
	}

	LTN_ASSERT(materialSlotCount < MeshInfo::MATERIAL_SLOT_COUNT_MAX);
	memcpy(meshInfo._materialSlotNameHashes, materialNameHashes, sizeof(ul64) * materialSlotCount);

	_meshStateFlags[meshIndex] |= MESH_FLAG_STATE_CHANGE;

	// resident メッシュの設定をロードしておく
	gpu::Mesh& mesh = _meshes[meshIndex];
	mesh._stateFlags = gpu::MESH_STATE_ALLOCATED;
	mesh._lodMeshCount = totalLodMeshCount;
	mesh._lodMeshOffset = lodMeshStartIndex;
	mesh._streamedLodLevel = totalLodMeshCount - 1;

	for (u32 lodMeshLocalIndex = 0; lodMeshLocalIndex < totalLodMeshCount; ++lodMeshLocalIndex) {
		u32 lodMeshIndex = lodMeshStartIndex + lodMeshLocalIndex;
		const LodInfo& info = lodInfos[lodMeshLocalIndex];
		gpu::LodMesh& lodMesh = _lodMeshes[lodMeshIndex];
		lodMesh._subMeshOffset = subMeshStartIndex + info._subMeshOffset;
		lodMesh._subMeshCount = info._subMeshCount;
		lodMesh._vertexOffset = info._vertexOffset;
		lodMesh._vertexIndexOffset = info._vertexIndexOffset;
		lodMesh._primitiveOffset = info._primitiveOffset;
	}

	for (u32 subMeshLocalIndex = 0; subMeshLocalIndex < totalSubMeshCount; ++subMeshLocalIndex) {
		u32 subMeshIndex = subMeshStartIndex + subMeshLocalIndex;
		const SubMeshInfoE& info = inputSubMeshInfos[subMeshLocalIndex];
		gpu::SubMesh& subMesh = _subMeshes[subMeshIndex];
		subMesh._meshletOffset = info._meshletStartIndex + meshletStartIndex;
		subMesh._meshletCount = info._meshletCount;
		subMesh._materialSlotIndex = info._materialSlotIndex;
	}

	// VRAM アップロード
	{
		gpu::Mesh* mapMesh = vramUpdater->enqueueUpdate<gpu::Mesh>(&_meshBuffer, sizeof(gpu::Mesh) * meshIndex);
		memcpy(mapMesh, &_meshes[meshIndex], sizeof(gpu::Mesh));
	}

	asset.updateCurrentSeekPosition();
	asset.closeFile();

	meshImpl->setAsset(&_meshAssets[meshIndex]);
	meshImpl->setMesh(&_meshes[meshIndex]);
	meshImpl->setLodMeshes(&_lodMeshes[lodMeshStartIndex]);
	meshImpl->setSubMeshes(&_subMeshes[subMeshStartIndex]);
	meshImpl->setMeshlets(&_meshlets[meshletStartIndex]);
	meshImpl->setMeshInfo(&_meshInfos[meshIndex]);
	meshImpl->setSubMeshInfos(&_subMeshInfos[subMeshStartIndex]);
	meshImpl->setStateFlags(&_meshStateFlags[meshIndex]);
	meshImpl->setDebugMeshInfo(&_debugMeshInfo[meshIndex]);

	return meshImpl;
}

Mesh* MeshResourceManager::createMesh(const MeshDesc& desc) {
	u64 fileHash = StrHash(desc._filePath);
	u32 searchedMeshIndex = getMeshIndexFromFileHash(fileHash);
	if (searchedMeshIndex != gpu::INVALID_INDEX) {
		return &_meshInstances[searchedMeshIndex];
	}

	MeshImpl* mesh = allocateMesh(desc);
	mesh->getAsset()->requestLoad();
	return mesh;
}

MeshImpl* MeshResourceManager::findMesh(u64 fileHash) {
	u32 meshIndex = getMeshIndexFromFileHash(fileHash);
	LTN_ASSERT(meshIndex != gpu::INVALID_INDEX);
	return &_meshInstances[meshIndex];
}

u32 MeshResourceManager::getMeshIndexFromFileHash(ul64 fileHash) const {
	u32 findMeshIndex = gpu::INVALID_INDEX;
	u32 meshCount = _meshes.getResarveCount();
	for (u32 meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
		if (_fileHashes[meshIndex] == fileHash) {
			findMeshIndex = meshIndex;
			break;
		}
	}

	return findMeshIndex;
}

u32 MeshResourceManager::getMeshIndex(const MeshInfo* meshInfo) const {
	u32 index = static_cast<u32>(meshInfo - _meshInfos);
	LTN_ASSERT(index < MESH_COUNT_MAX);
	return index;
}

GpuDescriptorHandle MeshResourceManager::getSubMeshSrv() const {
	u64 incrementSize = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator()->getIncrimentSize();
	return _meshSrv._gpuHandle + incrementSize * 2;
}

GpuDescriptorHandle MeshResourceManager::getVertexPositionSrv() const {
	u64 incrementSize = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator()->getIncrimentSize();
	return _vertexSrv._gpuHandle + incrementSize * 2;
}

MeshResourceManagerInfo MeshResourceManager::getMeshResourceInfo() const {
	MeshResourceManagerInfo info;
	info._meshCount = _meshes.getInstanceCount();
	info._lodMeshCount = _lodMeshes.getInstanceCount();
	info._subMeshCount = _subMeshes.getInstanceCount();
	info._meshletCount = _meshlets.getInstanceCount();
	info._vertexCount = _vertexPositionBinaryHeaders.getInstanceCount();
	info._triangleCount = _primitiveBinaryHeaders.getInstanceCount();
	return info;
}

void MeshResourceManager::loadLodMeshes(u32 meshIndex, u32 beginLodLevel, u32 endLodLevel) {
	Asset& asset = _meshAssets[meshIndex];
	asset.openFile(asset.getFilePath());
	FILE* fin = asset.getFile();
	fseek(fin, asset.getFileOffsetInByte(), SEEK_SET);

	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	const MeshInfo& meshInfo = _meshInfos[meshIndex];

	u32 beginSubMeshOffset = meshInfo._subMeshOffsets[beginLodLevel];
	u32 beginMeshletOffset = 0;
	u32 beginVertexOffset = 0;
	u32 beginIndexOffset = 0;
	u32 beginPrimitiveOffset = 0;
	u32 beginClassicIndexOffset = 0;
	for (u32 lodMeshIndex = 0; lodMeshIndex < beginLodLevel; ++lodMeshIndex) {
		beginMeshletOffset += meshInfo._meshletCounts[lodMeshIndex];
		beginVertexOffset += meshInfo._vertexCounts[lodMeshIndex];
		beginIndexOffset += meshInfo._vertexIndexCounts[lodMeshIndex];
		beginPrimitiveOffset += meshInfo._primitiveCounts[lodMeshIndex];
		beginClassicIndexOffset += meshInfo._classicIndexCounts[lodMeshIndex];
	}

	u32 endMeshletOffset = 0;
	u32 endVertexOffset = 0;
	u32 endIndexOffset = 0;
	u32 endPrimitiveOffset = 0;
	u32 endClassicIndexOffset = 0;
	for (u32 lodMeshIndex = endLodLevel; lodMeshIndex < meshInfo._totalLodMeshCount; ++lodMeshIndex) {
		endMeshletOffset += meshInfo._meshletCounts[lodMeshIndex];
		endVertexOffset += meshInfo._vertexCounts[lodMeshIndex];
		endIndexOffset += meshInfo._vertexIndexCounts[lodMeshIndex];
		endPrimitiveOffset += meshInfo._primitiveCounts[lodMeshIndex];
		endClassicIndexOffset += meshInfo._classicIndexCounts[lodMeshIndex];
	}

	u32 lodMeshCount = endLodLevel - beginLodLevel;
	u32 subMeshCount = 0;
	u32 meshletCount = 0;
	u32 vertexCount = 0;
	u32 indexCount = 0;
	u32 primitiveCount = 0;
	u32 classicIndexCount = 0;
	for (u32 lodMeshIndex = beginLodLevel; lodMeshIndex < endLodLevel; ++lodMeshIndex) {
		subMeshCount += meshInfo._subMeshCounts[lodMeshIndex];
		meshletCount += meshInfo._meshletCounts[lodMeshIndex];
		vertexCount += meshInfo._vertexCounts[lodMeshIndex];
		indexCount += meshInfo._vertexIndexCounts[lodMeshIndex];
		primitiveCount += meshInfo._primitiveCounts[lodMeshIndex];
		classicIndexCount += meshInfo._classicIndexCounts[lodMeshIndex];
	}

	u32 vertexOffsets[LOD_MESH_COUNT_MAX] = {};
	u32 indexOffsets[LOD_MESH_COUNT_MAX] = {};
	u32 primitiveOffsets[LOD_MESH_COUNT_MAX] = {};
	u32 classicIndexOffsets[LOD_MESH_COUNT_MAX] = {};
	for (u32 lodMeshIndex = beginLodLevel + 1; lodMeshIndex < endLodLevel; ++lodMeshIndex) {
		u32 prevLodMeshIndex = lodMeshIndex - 1;
		vertexOffsets[lodMeshIndex] = vertexOffsets[prevLodMeshIndex] + meshInfo._vertexCounts[prevLodMeshIndex];
		indexOffsets[lodMeshIndex] = indexOffsets[prevLodMeshIndex] + meshInfo._vertexIndexCounts[prevLodMeshIndex];
		primitiveOffsets[lodMeshIndex] = primitiveOffsets[prevLodMeshIndex] + meshInfo._primitiveCounts[prevLodMeshIndex];
		classicIndexOffsets[lodMeshIndex] = classicIndexOffsets[prevLodMeshIndex] + meshInfo._classicIndexCounts[prevLodMeshIndex];
	}

	std::vector<Vec3f> vertices(vertexCount);
	std::vector<Vec3ui> triangles(classicIndexCount / 3);
	u32 vertexBinaryIndex = _vertexPositionBinaryHeaders.request(vertexCount);
	u32 indexBinaryIndex = _indexBinaryHeaders.request(indexCount);
	u32 primitiveBinaryIndex = _primitiveBinaryHeaders.request(primitiveCount);
	// Meshlet
	{
		{
			u32 beginOffset = sizeof(gpu::MeshletPrimitiveInfo) * beginMeshletOffset;
			u32 endOffset = sizeof(gpu::MeshletPrimitiveInfo) * endMeshletOffset;
			gpu::MeshletPrimitiveInfo* meshletPrimitiveInfos = vramUpdater->enqueueUpdate<gpu::MeshletPrimitiveInfo>(&_meshletPrimitiveInfoBuffer, sizeof(gpu::MeshletPrimitiveInfo) * (meshInfo._meshletStartIndex + beginMeshletOffset), meshletCount);
			fseek(fin, beginOffset, SEEK_CUR);
			fread_s(meshletPrimitiveInfos, sizeof(gpu::MeshletPrimitiveInfo) * meshletCount, sizeof(gpu::MeshletPrimitiveInfo), meshletCount, fin);
			fseek(fin, endOffset, SEEK_CUR);
		}
		
		{
			u32 beginOffset = sizeof(gpu::Meshlet) * beginMeshletOffset;
			u32 endOffset = sizeof(gpu::Meshlet) * endMeshletOffset;
			gpu::Meshlet* meshlets = vramUpdater->enqueueUpdate<gpu::Meshlet>(&_meshletBuffer, sizeof(gpu::Meshlet) * (meshInfo._meshletStartIndex + beginMeshletOffset), meshletCount);
			fseek(fin, beginOffset, SEEK_CUR);
			fread_s(meshlets, sizeof(gpu::Meshlet) * meshletCount, sizeof(gpu::Meshlet), meshletCount, fin);
			fseek(fin, endOffset, SEEK_CUR);
		}
	}

	// プリミティブ
	u32* primitivePtr = nullptr;
	{
		u32 endOffset = sizeof(u32) * endPrimitiveOffset;
		u32 srcOffset = sizeof(u32) * beginPrimitiveOffset;
		u32 dstOffset = sizeof(u32) * primitiveBinaryIndex;
		primitivePtr = vramUpdater->enqueueUpdate<u32>(&_primitiveBuffer, dstOffset, primitiveCount);
		fseek(fin, srcOffset, SEEK_CUR);
		fread_s(primitivePtr, sizeof(u32) * primitiveCount, sizeof(u32), primitiveCount, fin);
		fseek(fin, endOffset, SEEK_CUR);
	}

	// 頂点インデックス
	u32* indexPtr = nullptr;
	{
		u32 endOffset = sizeof(u32) * endIndexOffset;
		u32 srcOffset = sizeof(u32) * beginIndexOffset;
		u32 dstOffset = sizeof(u32) * indexBinaryIndex;
		indexPtr = vramUpdater->enqueueUpdate<u32>(&_vertexIndexBuffer, dstOffset, indexCount);
		fseek(fin, srcOffset, SEEK_CUR);
		fread_s(indexPtr, sizeof(u32) * indexCount, sizeof(u32), indexCount, fin);
		fseek(fin, endOffset, SEEK_CUR);
	}

	// 座標
	{
		u32 endOffset = sizeof(VertexPosition) * endVertexOffset;
		u32 srcOffset = sizeof(VertexPosition) * beginVertexOffset;
		u32 dstOffset = sizeof(VertexPosition) * vertexBinaryIndex;
		VertexPosition* vertexPtr = vramUpdater->enqueueUpdate<VertexPosition>(&_positionVertexBuffer, dstOffset, vertexCount);
		fseek(fin, srcOffset, SEEK_CUR);
		fread_s(vertices.data(), sizeof(VertexPosition) * vertexCount, sizeof(VertexPosition), vertexCount, fin);
		fseek(fin, endOffset, SEEK_CUR);
		memcpy(vertexPtr, vertices.data(), sizeof(VertexPosition) * vertices.size());
	}

	// 法線 / 接線
	{
		u32 endOffset = sizeof(VertexNormalTangent) * endVertexOffset;
		u32 srcOffset = sizeof(VertexNormalTangent) * beginVertexOffset;
		u32 dstOffset = sizeof(VertexNormalTangent) * vertexBinaryIndex;
		VertexNormalTangent* normalPtr = vramUpdater->enqueueUpdate<VertexNormalTangent>(&_normalTangentVertexBuffer, dstOffset, vertexCount);
		fseek(fin, srcOffset, SEEK_CUR);
		fread_s(normalPtr, sizeof(VertexNormalTangent) * vertexCount, sizeof(VertexNormalTangent), vertexCount, fin);
		fseek(fin, endOffset, SEEK_CUR);
	}

	// UV
	{
		u32 endOffset = sizeof(VertexTexcoord) * endVertexOffset;
		u32 srcOffset = sizeof(VertexTexcoord) * beginVertexOffset;
		u32 dstOffset = sizeof(VertexTexcoord) * vertexBinaryIndex;
		VertexTexcoord* texcoordPtr = vramUpdater->enqueueUpdate<VertexTexcoord>(&_texcoordVertexBuffer, dstOffset, vertexCount);
		fseek(fin, srcOffset, SEEK_CUR);
		fread_s(texcoordPtr, sizeof(VertexTexcoord) * vertexCount, sizeof(VertexTexcoord), vertexCount, fin);
		fseek(fin, endOffset, SEEK_CUR);
	}

#if ENABLE_CLASSIC_VERTEX
	u32 classicIndex = _classicIndexBinaryHeaders.request(classicIndexCount);
	// Classic Index
	{
		u32 endOffset = sizeof(u32) * endClassicIndexOffset;
		u32 srcOffset = sizeof(u32) * beginClassicIndexOffset;
		u32 dstOffset = sizeof(u32) * classicIndex;
		u32* classicIndexPtr = vramUpdater->enqueueUpdate<u32>(&_classicIndexBuffer, dstOffset, classicIndexCount);
		fseek(fin, srcOffset, SEEK_CUR);
		fread_s(triangles.data(), sizeof(u32) * classicIndexCount, sizeof(u32), classicIndexCount, fin);
		fseek(fin, endOffset, SEEK_CUR);
		memcpy(classicIndexPtr, triangles.data(), sizeof(u32)* triangles.size());
	}

	u32 classicIndexLocalOffset = 0;
	SubMeshInfo* subMeshInfos = &_subMeshInfos[meshInfo._subMeshStartIndex + beginSubMeshOffset];
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		SubMeshInfo& info = subMeshInfos[subMeshIndex];
		info._classiciIndexOffset = classicIndex + classicIndexLocalOffset;
		classicIndexLocalOffset += info._classicIndexCount;
	}
#endif

	MeshInfo& updateMeshInfo = _meshInfos[meshIndex];
	for (u32 lodMeshIndex = beginLodLevel; lodMeshIndex < endLodLevel; ++lodMeshIndex) {
		u32 lodMeshOffset = meshInfo._lodMeshStartIndex + lodMeshIndex;
		gpu::LodMesh& lodMesh = _lodMeshes[lodMeshOffset];
		lodMesh._vertexOffset = vertexBinaryIndex + vertexOffsets[lodMeshIndex];
		lodMesh._vertexIndexOffset = indexBinaryIndex + indexOffsets[lodMeshIndex];
		lodMesh._primitiveOffset = primitiveBinaryIndex + primitiveOffsets[lodMeshIndex];

		updateMeshInfo._globalVertexOffsets[lodMeshIndex] = vertexBinaryIndex + vertexOffsets[lodMeshIndex];
		updateMeshInfo._globalIndexOffsets[lodMeshIndex] = indexBinaryIndex + indexOffsets[lodMeshIndex];
		updateMeshInfo._globalPrimitiveOffsets[lodMeshIndex] = primitiveBinaryIndex + primitiveOffsets[lodMeshIndex];
#if ENABLE_CLASSIC_VERTEX
		updateMeshInfo._globalClassicIndexOffsets[lodMeshIndex] = classicIndex + classicIndexOffsets[lodMeshIndex];
#endif
	}

	// データVramアップロード
	u32 subMeshOffset = sizeof(gpu::SubMesh) * (meshInfo._subMeshStartIndex + beginSubMeshOffset);
	u32 lodMeshOffset = sizeof(gpu::LodMesh) * (meshInfo._lodMeshStartIndex + beginLodLevel);
	gpu::SubMesh* subMeshes = vramUpdater->enqueueUpdate<gpu::SubMesh>(&_subMeshBuffer, subMeshOffset, subMeshCount);
	gpu::LodMesh* lodMeshes = vramUpdater->enqueueUpdate<gpu::LodMesh>(&_lodMeshBuffer, lodMeshOffset, lodMeshCount);
	memcpy(subMeshes, &_subMeshes[meshInfo._subMeshStartIndex + beginSubMeshOffset], sizeof(gpu::SubMesh) * subMeshCount);
	memcpy(lodMeshes, &_lodMeshes[meshInfo._lodMeshStartIndex + beginLodLevel], sizeof(gpu::LodMesh) * lodMeshCount);

	gpu::SubMeshDrawInfo* subMeshDrawInfos = vramUpdater->enqueueUpdate<gpu::SubMeshDrawInfo>(&_subMeshDrawInfoBuffer, sizeof(gpu::SubMeshDrawInfo) * (meshInfo._subMeshStartIndex + beginSubMeshOffset), subMeshCount);
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		const SubMeshInfo& subMeshInfo = subMeshInfos[subMeshIndex];
		gpu::SubMeshDrawInfo& info = subMeshDrawInfos[subMeshIndex];
		info._indexCount = subMeshInfo._classicIndexCount;
		info._indexOffset = subMeshInfo._classiciIndexOffset;
	}

	// 読み込み完了
	_meshAssets[meshIndex].closeFile();

#define SDF_GENERATE_GPU 1

	// SDF
	// もっとも粗いLODLevelのロード時にSDFを生成します
	if (endLodLevel == meshInfo._totalLodMeshCount) {
		GpuTexture& texture = _meshSdfs[meshIndex];
		Device* device = GraphicsSystemImpl::Get()->getDevice();
		GpuTextureDesc textureDesc = {};
		textureDesc._device = device;
		textureDesc._format = FORMAT_R8_SNORM;
		textureDesc._dimension = RESOURCE_DIMENSION_TEXTURE3D;
		textureDesc._width = meshInfo._sdfResolution[0];
		textureDesc._height = meshInfo._sdfResolution[1];
		textureDesc._depthOrArraySize = meshInfo._sdfResolution[2];
		textureDesc._mipLevels = 1;
		textureDesc._initialState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		textureDesc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		texture.initialize(textureDesc);

		char debugName[128];
		sprintf_s(debugName, "SDF %s", _debugMeshInfo[meshIndex]._filePath);
		texture.setDebugName(debugName);

#if SDF_GENERATE_GPU
		_sdfGenerator.enqueue(&meshInfo, meshIndex);
#else
		u32 lodLevel = endLodLevel - 1;
		std::vector<Vec3f> sdfVertices(meshInfo._vertexCounts[lodLevel]);
		std::vector<Vec3ui> sdfTriangles(meshInfo._classicIndexCounts[lodLevel] / 3);
		memcpy(sdfVertices.data(), vertices.data() + vertexOffsets[lodLevel], sizeof(Vec3f)* sdfVertices.size());
		memcpy(sdfTriangles.data(), triangles.data() + classicIndexOffsets[lodLevel] / 3, sizeof(Vec3ui) * sdfTriangles.size());

		Vector3 origin = meshInfo._sdfBoundsMin + Vector3::One * meshInfo._sdfGridSize * 0.5f;
		Vec3f min_box(origin._x, origin._y, origin._z);

		Array3c distances;
		make_level_set3(sdfTriangles, sdfVertices, min_box, meshInfo._sdfGridSize, meshInfo._sdfResolution[0], meshInfo._sdfResolution[1], meshInfo._sdfResolution[2], distances);

		u32 subResourceIndex = 0;
		u32 numberOfPlanes = device->getFormatPlaneCount(textureDesc._format);
		SubResourceData subResources[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
		for (u32 planeIndex = 0; planeIndex < numberOfPlanes; ++planeIndex) {
			u64 srcBits = 0;
			for (u32 arrayIndex = 0; arrayIndex < textureDesc._depthOrArraySize; arrayIndex++) {
				u32 w = static_cast<u32>(textureDesc._width);
				u32 h = static_cast<u32>(textureDesc._height);
				u32 d = static_cast<u32>(textureDesc._depthOrArraySize);
				for (u32 mipIndex = 0; mipIndex < textureDesc._mipLevels; mipIndex++) {
					u32 bpp = 32;// 4byte
					u32 numRow = h;
					u32 rowBytes = (w * bpp + 7u) / 8u;
					u32 numBytes = rowBytes * h;

					SubResourceData& subResource = subResources[subResourceIndex++];
					subResource._data = reinterpret_cast<const void*>(srcBits);
					subResource._rowPitch = static_cast<s64>(rowBytes);
					subResource._slicePitch = static_cast<s64>(numBytes);

					// AdjustPlaneResource(ddsHeaderDxt10._dxgiFormat, h, planeIndex, subResource);
					srcBits += numBytes * d;

					w = Max(w / 2, 1u);
					h = Max(h / 2, 1u);
					d = Max(d / 2, 1u);
				}
			}
		}

		if (subResourceIndex >= TextureUpdateHeader::SUBRESOURCE_COUNT_MAX) {
			int y = 0;
		}
		LTN_ASSERT(subResourceIndex < TextureUpdateHeader::SUBRESOURCE_COUNT_MAX);
		u64 requiredSize = 0;
		PlacedSubresourceFootprint layouts[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
		u32 numRows[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
		u64 rowSizesInBytes[TextureUpdateHeader::SUBRESOURCE_COUNT_MAX] = {};
		u32 numberOfResources = numberOfPlanes;
		ResourceDesc textureResourceDesc = texture.getResourceDesc();
		device->getCopyableFootprints(&textureResourceDesc, 0, numberOfResources, 0, layouts, numRows, rowSizesInBytes, &requiredSize);

		VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
		void* pixelData = vramUpdater->enqueueUpdate(&texture, numberOfResources, subResources, static_cast<u32>(requiredSize));

		u8* sourcePtr = reinterpret_cast<u8*>(&distances.a[0]);
		for (u32 subResourceIndex = 0; subResourceIndex < numberOfResources; ++subResourceIndex) {
			const PlacedSubresourceFootprint& layout = layouts[subResourceIndex];
			u8* data = reinterpret_cast<u8*>(pixelData) + layout._offset;
			u64 rowSizeInBytes = rowSizesInBytes[subResourceIndex];
			u32 numRow = numRows[subResourceIndex];
			u32 rowPitch = layout._footprint._rowPitch;
			u32 slicePitch = layout._footprint._rowPitch * numRow;
			u32 numSlices = layout._footprint._depth;
			for (u32 z = 0; z < numSlices; ++z) {
				u64 slicePitchOffset = static_cast<u64>(slicePitch) * z;
				u8* destSlice = data + slicePitchOffset;
				for (u32 y = 0; y < numRow; ++y) {
					u64 rowPitchOffset = static_cast<u64>(rowPitch) * y;
					memcpy(destSlice + rowPitchOffset, sourcePtr, sizeof(u8) * rowSizeInBytes);
					sourcePtr += sizeof(u8) * rowSizeInBytes;
				}
			}
		}

#endif
		u32 incSize = static_cast<u64>(GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator()->getIncrimentSize());
		device->createShaderResourceView(texture.getResource(), nullptr, _meshSdfSrv._cpuHandle + incSize * meshIndex);
		device->createUnorderedAccessView(texture.getResource(), nullptr, nullptr, _meshSdfUav._cpuHandle + incSize * meshIndex);

		Vector3 boundsSize = meshInfo._sdfBundsMax - meshInfo._sdfBoundsMin;
		gpu::MeshBounds meshBounds;
		meshBounds._meshBounds = boundsSize.getFloat3();

		gpu::MeshBounds* mapMeshBounds = vramUpdater->enqueueUpdate<gpu::MeshBounds>(&_meshBoundsSizeBuffer, sizeof(gpu::MeshBounds) * meshIndex);
		*mapMeshBounds = meshBounds;
	}
}

void MeshResourceManager::deleteMesh(u32 meshIndex) {
	const MeshInfo& meshInfo = _meshInfos[meshIndex];
	deleteLodMeshes(meshIndex, 0, meshInfo._totalLodMeshCount);

	_meshlets.discard(&_meshlets[meshInfo._meshletStartIndex], meshInfo._totalMeshletCount);
	_subMeshes.discard(&_subMeshes[meshInfo._subMeshStartIndex], meshInfo._totalSubMeshCount);
	_lodMeshes.discard(&_lodMeshes[meshInfo._lodMeshStartIndex], meshInfo._totalLodMeshCount);
	_meshes.discard(meshIndex);

	_assetStateFlags[meshIndex] = ASSET_STATE_NONE;
	_meshStateFlags[meshIndex] = MESH_FLAG_STATE_NONE;
	_fileHashes[meshIndex] = 0;
	_meshInfos[meshIndex] = MeshInfo();
	_meshInstances[meshIndex] = MeshImpl();
	_debugMeshInfo[meshIndex] = DebugMeshInfo();

	for (u32 subMeshIndex = 0; subMeshIndex < meshInfo._totalSubMeshCount; ++subMeshIndex) {
		_subMeshInfos[meshInfo._subMeshStartIndex + subMeshIndex] = SubMeshInfo();
	}

	_meshSdfs[meshIndex].terminate();
}

void MeshResourceManager::deleteLodMeshes(u32 meshIndex, u32 beginLodLevel, u32 endLodLevel) {
	u32 vertexCount = 0;
	u32 indexCount = 0;
	u32 primitiveCount = 0;
	u32 classicIndexCount = 0;
	const MeshInfo& meshInfo = _meshInfos[meshIndex];
	for (u32 lodMeshIndex = beginLodLevel; lodMeshIndex < endLodLevel; ++lodMeshIndex) {
		vertexCount += meshInfo._vertexCounts[lodMeshIndex];
		indexCount += meshInfo._vertexIndexCounts[lodMeshIndex];
		primitiveCount += meshInfo._primitiveCounts[lodMeshIndex];
		classicIndexCount += meshInfo._classicIndexCounts[lodMeshIndex];
	}

	_vertexPositionBinaryHeaders.discard(meshInfo._globalVertexOffsets[beginLodLevel], vertexCount);
	_indexBinaryHeaders.discard(meshInfo._globalIndexOffsets[beginLodLevel], indexCount);
	_primitiveBinaryHeaders.discard(meshInfo._globalPrimitiveOffsets[beginLodLevel], primitiveCount);
	_classicIndexBinaryHeaders.discard(meshInfo._globalClassicIndexOffsets[beginLodLevel], classicIndexCount);
}

void Mesh::requestToDelete(){
	*_stateFlags |= MESH_FLAG_STATE_REQUEST_DESTROY;
}

void MeshSdfGenerator::initialize() {
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
		{
			GpuBufferDesc desc = {};
			desc._sizeInByte = GetConstantBufferAligned(sizeof(ComputeSdfConstant));
			desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			desc._device = device;
			_computeMeshSdfConstantBuffer.initialize(desc);
			_computeMeshSdfConstantBuffer.setDebugName("Compute Sdf Constant");
		}

		{
			DescriptorRange sdfCbvRange(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			DescriptorRange indicesSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			DescriptorRange vertexPositionSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			DescriptorRange outSdfUavRange(DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

			_computeMeshSdfRootSignature = allocator->allocateRootSignature();

			RootParameter rootParameters[ComputeSdfRootParam::COUNT] = {};
			rootParameters[ComputeSdfRootParam::COMPUTE_INFO].initializeDescriptorTable(1, &sdfCbvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ComputeSdfRootParam::INDICES].initializeDescriptorTable(1, &indicesSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ComputeSdfRootParam::VERTEX_POSITION].initializeDescriptorTable(1, &vertexPositionSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ComputeSdfRootParam::OUT_SDF].initializeDescriptorTable(1, &outSdfUavRange, SHADER_VISIBILITY_ALL);

			RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			_computeMeshSdfRootSignature->iniaitlize(rootSignatureDesc);
			_computeMeshSdfRootSignature->setDebugName("Gpu Culling");
		}

		{
			ShaderBlob* computeShader = allocator->allocateShaderBlob();
			computeShader->initialize("L:/LightnEngine/resource/common/shader/sdf/compute_mesh_sdf.cso");

			ComputePipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._rootSignature = _computeMeshSdfRootSignature;
			pipelineStateDesc._cs = computeShader->getShaderByteCode();

			_computeMeshSdfPipelineState = allocator->allocatePipelineState();
			_computeMeshSdfPipelineState->iniaitlize(pipelineStateDesc);
			_computeMeshSdfPipelineState->setDebugName("L:/LightnEngine/resource/common/shader/sdf/compute_mesh_sdf.cso");

			computeShader->terminate();
		}
	}

	// デスクリプタ
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_computeMeshSdfCbv = allocator->allocateDescriptors(1);
		device->createConstantBufferView(_computeMeshSdfConstantBuffer.getConstantBufferViewDesc(), _computeMeshSdfCbv._cpuHandle);
	}
}

void MeshSdfGenerator::terminate() {
	_computeMeshSdfConstantBuffer.terminate();
	_computeMeshSdfPipelineState->terminate();
	_computeMeshSdfRootSignature->terminate();

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_computeMeshSdfCbv);
}

void MeshSdfGenerator::enqueue(const MeshInfo* meshInfo, u32 meshIndex) {
	LTN_ASSERT(_queueCount < PROCESS_QUEUE_COUNT_MAX - 1);
	_processQueue[_queueCount++] = meshInfo;
	_processMeshIndex = meshIndex;
}

void MeshSdfGenerator::update() {
	if (_queueCount == 0) {
		return;
	}

	// もっとも粗いLODを指定
	const MeshInfo* meshInfo = _processQueue[--_queueCount];
	u32 lodOffset = meshInfo->_totalLodMeshCount - 1;
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	ComputeSdfConstant* constant = vramUpdater->enqueueUpdate<ComputeSdfConstant>(&_computeMeshSdfConstantBuffer, 0);
	constant->_resolution[0] = meshInfo->_sdfResolution[0];
	constant->_resolution[1] = meshInfo->_sdfResolution[1];
	constant->_resolution[2] = meshInfo->_sdfResolution[2];
	constant->_sdfBoundsMin = meshInfo->_sdfBoundsMin.getFloat3();
	constant->_sdfBoundsMax = meshInfo->_sdfBundsMax.getFloat3();
	constant->_indexOffset = meshInfo->_globalClassicIndexOffsets[lodOffset];
	constant->_indexCount = meshInfo->_classicIndexCounts[lodOffset];

	_processVoxelCount = meshInfo->_sdfResolution[0] * meshInfo->_sdfResolution[1] * meshInfo->_sdfResolution[2];
}

void MeshSdfGenerator::processComputeMeshSdf(const ProcessContext& context) {
	if (_processVoxelCount == 0) {
		return;
	}

	u64 incrementSize = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator()->getIncrimentSize();
	u64 meshSdfOffset = incrementSize * _processMeshIndex;
	CommandList* commandList = context._commandList;
	commandList->setComputeRootSignature(_computeMeshSdfRootSignature);
	commandList->setPipelineState(_computeMeshSdfPipelineState);
	commandList->setComputeRootDescriptorTable(ComputeSdfRootParam::COMPUTE_INFO, _computeMeshSdfCbv._gpuHandle);
	commandList->setComputeRootDescriptorTable(ComputeSdfRootParam::INDICES, context._classicIndexSrv);
	commandList->setComputeRootDescriptorTable(ComputeSdfRootParam::VERTEX_POSITION, context._vertexPositionSrv);
	commandList->setComputeRootDescriptorTable(ComputeSdfRootParam::OUT_SDF, context._meshSdfUav + meshSdfOffset);
	
	u32 dispatchCountX = (_processVoxelCount / 32) + 1;
	commandList->dispatch(dispatchCountX, 1, 1);

	_processVoxelCount = 0;
	_processMeshIndex = 0;
}
