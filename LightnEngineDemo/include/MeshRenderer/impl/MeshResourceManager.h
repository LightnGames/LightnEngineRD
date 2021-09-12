#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>
#include <MeshRenderer/MeshRendererSystem.h>
#include <MeshRenderer/GpuStruct.h>

struct CommandList;

enum VertexBufferViewLayout {
	VERTEX_BUFFER_VIEW_LAYOUT_POSITION = 0,
	VERTEX_BUFFER_VIEW_LAYOUT_NORMAL,
	VERTEX_BUFFER_VIEW_LAYOUT_TANGENT,
	VERTEX_BUFFER_VIEW_LAYOUT_TEXCOORD,
	VERTEX_BUFFER_VIEW_LAYOUT_COLOR,
	VERTEX_BUFFER_VIEW_LAYOUT_COUNT,
};

enum MeshStateFlags {
	MESH_FLAG_STATE_NONE = 0,
	MESH_FLAG_STATE_CHANGE = 1 << 0,
	MESH_FLAG_STATE_REQUEST_DESTROY = 1 << 1,
};

class MeshImpl :public Mesh {
public:
	void setAsset(Asset* asset) {
		_asset = asset;
	}

	void setStateFlags(u8* stateFlags) {
		_stateFlags = stateFlags;
	}

	void setMesh(const gpu::Mesh* mesh) {
		_gpuMesh = mesh;
	}

	void setLodMeshes(const gpu::LodMesh* lodMeshes) {
		_gpuLodMeshes = lodMeshes;
	}

	void setSubMeshes(const gpu::SubMesh* subMeshes) {
		_gpuSubMeshes = subMeshes;
	}

	void setMeshlets(const gpu::Meshlet* meshlets) {
		_gpuMeshlets = meshlets;
	}

	void setMeshInfo(const MeshInfo* meshInfo) {
		_meshInfo = meshInfo;
	}

	void setSubMeshInfos(const SubMeshInfo* subMeshInfos) {
		_subMeshInfos = subMeshInfos;
	}

	void setDebugMeshInfo(const DebugMeshInfo* meshInfo) {
		_debugMeshInfo = meshInfo;
	}
};

struct MeshSdfDimension {
	u32 _sizes[3] = {};
};

struct MeshResourceManagerInfo {
	u32 _meshCount = 0;
	u32 _lodMeshCount = 0;
	u32 _subMeshCount = 0;
	u32 _meshletCount = 0;
	u32 _vertexCount = 0;
	u32 _triangleCount = 0;
};

class MeshSdfGenerator {
public:
	static constexpr u32 PROCESS_QUEUE_COUNT_MAX = 256;
	struct ProcessContext {
		CommandList* _commandList = nullptr;
		GpuDescriptorHandle _classicIndexSrv;
		GpuDescriptorHandle _vertexPositionSrv;
		GpuDescriptorHandle _meshSdfUav;
	};

	struct ComputeSdfConstant {
		u32 _resolution[3];
		u32 _indexCount;
		Float3 _sdfBoundsMin;
		u32 _indexOffset;
		Float3 _sdfBoundsMax;
		u32 _vertexOffset;
		f32 _cellSize;
	};

	struct QueueData {
		const MeshInfo* _meshInfo = nullptr;
		GpuTexture* _processSdfTexture = nullptr;
		u32 _processMeshIndex = 0;
		u32 _processVoxelCount = 0;
	};

	void initialize();
	void terminate();

	void enqueue(const MeshInfo* meshInfo, u32 meshIndex, GpuTexture* sdfTexture);

	void update();
	void processComputeMeshSdf(const ProcessContext& context);

private:
	u32 _queueCount = 0;
	QueueData _processQueue[PROCESS_QUEUE_COUNT_MAX] = {};

	GpuBuffer _computeMeshSdfConstantBuffer;
	DescriptorHandle _computeMeshSdfCbv;

	PipelineState* _computeMeshSdfPipelineState = nullptr;
	RootSignature* _computeMeshSdfRootSignature = nullptr;
};

class MeshResourceManager {
public:
	static constexpr u32 MESH_COUNT_MAX = 256;
	static constexpr u32 LOD_MESH_COUNT_MAX = 1024;
	static constexpr u32 SUB_MESH_COUNT_MAX = 1024 * 4;
	static constexpr u32 MESHLET_COUNT_MAX = 1024 * 256;
	static constexpr u32 VERTEX_COUNT_MAX = 1024 * 1024 * 2;
	static constexpr u32 INDEX_COUNT_MAX = 1024 * 1024 * 2;
	static constexpr u32 PRIMITIVE_COUNT_MAX = 1024 * 1024 * 2;
	using VertexPosition = Float3;
	using VertexNormalTangent = u32;
	using VertexTexcoord = u32;
	using VertexColor = u32;

	void initialize();
	void update();
	void processDeletion();
	void terminate();
	void terminateDefaultResources();
	void drawDebugGui();
	void loadMesh(u32 meshIndex);

	MeshImpl* allocateMesh(const MeshDesc& desc);
	Mesh* createMesh(const MeshDesc& desc);
	MeshImpl* findMesh(u64 fileHash);
	const gpu::SubMesh* getSubMeshes() const { return &_subMeshes[0]; }
	u32 getMeshIndexFromFileHash(u64 fileHash) const;
	u32 getMeshIndex(const MeshInfo* meshInfo) const;
	GpuDescriptorHandle getMeshSrv() const { return _meshSrv._gpuHandle; }
	GpuDescriptorHandle getSubMeshSrv() const;
	GpuDescriptorHandle getVertexSrv() const { return _vertexSrv._gpuHandle; }
	GpuDescriptorHandle getVertexPositionSrv() const;
	GpuDescriptorHandle getClassicIndexSrv() const{ return _classicIndexSrv._gpuHandle; }
	GpuDescriptorHandle getMeshSdfUav() const { return _meshSdfUav._gpuHandle; }
	GpuBuffer* getPositionVertexBuffer() { return &_positionVertexBuffer; }
	GpuBuffer* getNormalVertexBuffer() { return &_normalTangentVertexBuffer; }
	GpuBuffer* getTexcoordVertexBuffer() { return &_texcoordVertexBuffer; }
	GpuBuffer* getMeshLodLevelFeedbackBuffer() { return &_meshLodLevelFeedbackBuffer; }
	GpuBuffer* getMeshLodLevelFeedbackReadbackBuffer() { return &_meshLodLevelFeedbackReadbackBuffer; }
	MeshResourceManagerInfo getMeshResourceInfo() const;
	MeshSdfGenerator* getMeshSdfGenerator() { return &_sdfGenerator; }

#if ENABLE_CLASSIC_VERTEX
	GpuBuffer* getClassicIndexBuffer() { return &_classicIndexBuffer; }
#endif

	GpuDescriptorHandle getSubMeshDrawInfoSrv() const { return _subMeshDrawInfoSrv._gpuHandle; }
	GpuDescriptorHandle getMeshLodLevelFeedbackUav() const { return _meshLodLevelFeedbackUav._gpuHandle; }
	CpuDescriptorHandle getMeshLodLevelFeedbackCpuUav() const { return _meshLodLevelFeedbackCpuUav._cpuHandle; }
	GpuDescriptorHandle getMeshSdfSrv() const { return _meshSdfSrv._gpuHandle; }
	GpuDescriptorHandle getMeshBoundsSizeSrv() const { return _meshBoundsSizeSrv._gpuHandle; }
	bool isEnabledDebugDraw() { return _meshSdfs[1].getResource() != nullptr; }

private:
	void loadLodMeshes(u32 meshIndex, u32 beginLodLevel, u32 endLodLevel);
	void deleteMesh(u32 meshIndex);
	void deleteLodMeshes(u32 meshIndex, u32 beginLodLevel, u32 endLodLevel);

private:
	MeshSdfGenerator _sdfGenerator;
	Asset _meshAssets[MESH_COUNT_MAX] = {};
	MeshImpl _meshInstances[MESH_COUNT_MAX] = {};
	MeshInfo _meshInfos[MESH_COUNT_MAX] = {};
	SubMeshInfo _subMeshInfos[SUB_MESH_COUNT_MAX] = {};
	DebugMeshInfo _debugMeshInfo[MESH_COUNT_MAX] = {};
	u64 _fileHashes[MESH_COUNT_MAX] = {};
	u8 _meshStateFlags[MESH_COUNT_MAX] = {};
	u8 _assetStateFlags[MESH_COUNT_MAX] = {};

	DynamicQueue<gpu::Mesh> _meshes;
	MultiDynamicQueue<gpu::LodMesh> _lodMeshes;
	MultiDynamicQueue<gpu::SubMesh> _subMeshes;
	MultiDynamicQueue<gpu::Meshlet> _meshlets;

	MultiDynamicQueueBlockManager _vertexPositionBinaryHeaders;
	MultiDynamicQueueBlockManager _indexBinaryHeaders;
	MultiDynamicQueueBlockManager _primitiveBinaryHeaders;
	GpuBuffer _vertexIndexBuffer;
	GpuBuffer _primitiveBuffer;
	GpuBuffer _positionVertexBuffer;
	GpuBuffer _normalTangentVertexBuffer;
	GpuBuffer _texcoordVertexBuffer;

	GpuBuffer _meshBuffer;
	GpuBuffer _lodMeshBuffer;
	GpuBuffer _subMeshBuffer;
	GpuBuffer _meshletBuffer;
	GpuBuffer _meshletPrimitiveInfoBuffer;
	GpuBuffer _meshLodLevelFeedbackBuffer;
	GpuBuffer _meshLodLevelFeedbackReadbackBuffer;
	GpuBuffer _meshBoundsSizeBuffer;
	GpuTexture _meshSdfs[MESH_COUNT_MAX] = {};

	DescriptorHandle _meshSrv;
	DescriptorHandle _vertexSrv;
	DescriptorHandle _meshLodLevelFeedbackUav;
	DescriptorHandle _meshLodLevelFeedbackCpuUav;
	DescriptorHandle _meshSdfSrv;
	DescriptorHandle _meshSdfUav;
	DescriptorHandle _meshBoundsSizeSrv;
	Mesh* _defaultCube = nullptr;
	u32 _meshLodLevelFeedbacks[MESH_COUNT_MAX] = {};

#if ENABLE_CLASSIC_VERTEX
	MultiDynamicQueueBlockManager _classicIndexBinaryHeaders;
	GpuBuffer _classicIndexBuffer;
	DescriptorHandle _classicIndexSrv;
#endif
	GpuBuffer _subMeshDrawInfoBuffer;
	DescriptorHandle _subMeshDrawInfoSrv;
};