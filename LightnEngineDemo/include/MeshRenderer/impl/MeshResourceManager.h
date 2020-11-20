#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>
#include <MeshRenderer/MeshRendererSystem.h>
#include <MeshRenderer/GpuStruct.h>

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

class MeshResourceManager {
public:
	static constexpr u32 MESH_COUNT_MAX = 256;
	static constexpr u32 LOD_MESH_COUNT_MAX = 1024;
	static constexpr u32 SUB_MESH_COUNT_MAX = 2048;
	static constexpr u32 MESHLET_COUNT_MAX = 1024 * 1024;
	static constexpr u32 VERTEX_COUNT_MAX = 1024 * 1024 * 16;
	static constexpr u32 INDEX_COUNT_MAX = 1024 * 1024 * 32;
	static constexpr u32 PRIMITIVE_COUNT_MAX = 1024 * 1024 * 32;
	using VertexPosition = Float3;
	using VertexNormalTangent = u32;
	using VertexTexcoord = u32;
	using VertexColor = u32;

	void initialize();
	void update();
	void processDeletion();
	void terminate();
	void drawDebugGui();
	void loadMesh(u32 meshIndex);

	MeshImpl* allocateMesh(const MeshDesc& desc);
	Mesh* createMesh(const MeshDesc& desc);
	MeshImpl* findMesh(u64 fileHash);
	u32 getMeshIndexFromFileHash(u64 fileHash) const;
	u32 getMeshIndex(const MeshInfo* meshInfo) const;
	DescriptorHandle getMeshHandle() const { return _meshHandles; }
	DescriptorHandle getVertexHandle() const { return _vertexHandles; }
	GpuBuffer* getPositionVertexBuffer() { return &_positionVertexBuffer; }
	GpuBuffer* getNormalVertexBuffer() { return &_normalTangentVertexBuffer; }
	GpuBuffer* getTexcoordVertexBuffer() { return &_texcoordVertexBuffer; }

#if ENABLE_CLASSIC_VERTEX
	GpuBuffer* getClassicIndexBuffer() { return &_classicIndexBuffer; }
#endif

#if ENABLE_MULTI_INDIRECT_DRAW
	DescriptorHandle getSubMeshInfoSrvHandle() const { return _subMeshDrawInfoSrv; }
#endif

private:
	void deleteMesh(u32 meshIndex);

private:
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
	MultiDynamicQueueBlockManager _meshlets;

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

	DescriptorHandle _meshHandles;
	DescriptorHandle _vertexHandles;
	Mesh* _defaultCube = nullptr;

#if ENABLE_CLASSIC_VERTEX
	MultiDynamicQueueBlockManager _classicIndexBinaryHeaders;
	GpuBuffer _classicIndexBuffer;
#endif
#if ENABLE_MULTI_INDIRECT_DRAW
	GpuBuffer _subMeshDrawInfoBuffer;
	DescriptorHandle _subMeshDrawInfoSrv;
#endif
};