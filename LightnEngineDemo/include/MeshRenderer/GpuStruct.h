#pragma once
#include <Core/System.h>

namespace gpu {
	constexpr u32 INVALID_INDEX = 0xffffffff;
	constexpr u32 HIERACHICAL_DEPTH_COUNT = 8;
	constexpr u32 SHADER_SET_COUNT_MAX = 32;

	enum MeshState {
		MESH_STATE_NONE = 0,
		MESH_STATE_ALLOCATED,
		MESH_STATE_LOADED,
	};

	struct MeshletInstanceInfo {
		u32 _meshletIndex;
		u32 _meshInstanceIndex;
		u32 _materialIndex;
		u32 _vertexOffset;
		u32 _vertexIndexOffset;
		u32 _primitiveOffset;
	};

	struct Mesh {
		u32 _stateFlags = 0;
		u32 _lodMeshOffset = 0;
		u32 _lodMeshCount = 0;
	};

	struct LodMesh {
		u32 _vertexOffset = 0;
		u32 _vertexIndexOffset = 0;
		u32 _primitiveOffset = 0;
		u32 _subMeshOffset = 0;
		u32 _subMeshCount = 0;
	};

	struct SubMesh {
		u32 _meshletOffset = 0;
		u32 _meshletCount = 0;
		u32 _materialSlotIndex = 0;
	};

	struct SubMeshDrawInfo {
		u32 _indexCount = 0;
		u32 _indexOffset = 0;
	};

	struct Meshlet {
		u32 _vertexOffset = 0;
		u32 _vertexCount = 0;
		u32 _primitiveOffset = 0;
		u32 _primitiveCount = 0;
		Float3 _aabbMin;
		Float3 _aabbMax;
		u32 _normalAndCutoff;
		f32 _apexOffset = 0;
	};

	struct MeshletPrimitiveInfo {
		u32 _vertexOffset;
		u32 _vertexCount;
		u32 _primitiveOffset;
		u32 _primitiveCount;
	};

	struct MeshletInstancePrimitiveInfo {
		u32 _materialIndex;
		u32 _vertexOffset;
		u32 _vertexIndexOffset;
		u32 _primitiveOffset;
	};

	struct MeshInstance {
		u32 _stateFlags;
		u32 _meshIndex = 0;
		u32 _lodMeshInstanceOffset = 0;
		f32 _boundsRadius;
		Float3 _aabbMin;
		Float3 _aabbMax;
		Matrix4 _matrixWorld;
		f32 _worldScale = 0.0f;
	};

	struct LodMeshInstance {
		u32 _subMeshInstanceOffset = 0;
		f32 _threshhold = 0.0f;
	};

	struct SubMeshInstance {
		u32 _materialIndex = 0;
		u32 _shaderSetIndex = 0;
	};

	struct DispatchMeshIndirectArgumentAS {
		u32 _meshletInstanceInfoOffset = 0;
		u32 _instanceCount = 0;
		u32 _meshletCount = 0;
		u32 _dispatchX = 0;
		u32 _dispatchY = 0;
		u32 _dispatchZ = 0;
	};

	struct DispatchMeshIndirectArgumentMS {
		u32 _meshletInstanceInfoOffset = 0;
		u32 _instanceCount = 0;
		u32 _meshletCount = 0;
		u32 _meshletOffset = 0;
		u32 _dispatchX = 0;
		u32 _dispatchY = 0;
		u32 _dispatchZ = 0;
	};

	struct StarndardMeshIndirectArguments {
		u32 _meshInstanceIndex = 0;
		u32 _materialIndex = 0;
		u32 _indexCountPerInstance;
		u32 _instanceCount;
		u32 _startIndexLocation;
		s32 _baseVertexLocation;
		u32 _startInstanceLocation;
	};

	struct CullingResult {
		u32 _testFrustumCullingSubMeshInstanceCount = 0;
		u32 _passFrustumCullingSubMeshInstanceCount = 0;
		u32 _testOcclusionCullingSubMeshInstanceCount = 0;
		u32 _passOcclusionCullingSubMeshInstanceCount = 0;
		u32 _testOcclusionCullingMeshInstanceCount = 0;
		u32 _passOcclusionCullingMeshInstanceCount = 0;
		u32 _testFrustumCullingMeshInstanceCount = 0;
		u32 _passFrustumCullingMeshInstanceCount = 0;
		u32 _testFrustumCullingMeshletInstanceCount = 0;
		u32 _passFrustumCullingMeshletInstanceCount = 0;
		u32 _testBackfaceCullingMeshletInstanceCount = 0;
		u32 _passBackfaceCullingMeshletInstanceCount = 0;
		u32 _testOcclusionCullingMeshletInstanceCount = 0;
		u32 _passOcclusionCullingMeshletInstanceCount = 0;
		u32 _testFrustumCullingTriangleCount = 0;
		u32 _passFrustumCullingTriangleCount = 0;
		u32 _testBackfaceCullingTriangleCount = 0;
		u32 _passBackfaceCullingTriangleCount = 0;
		u32 _testOcclusionCullingTriangleCount = 0;
		u32 _passOcclusionCullingTriangleCount = 0;
	};
}