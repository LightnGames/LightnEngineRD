#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>
#include <MeshRenderer/MeshRendererSystem.h>
#include <MeshRenderer/impl/MeshRenderer.h>
#include <MeshRenderer/GpuStruct.h>

enum MeshInstanceStateFlag {
	MESH_INSTANCE_FLAG_NONE = 0,
	MESH_INSTANCE_FLAG_SCENE_ENQUEUED = 1 << 0,
	MESH_INSTANCE_FLAG_SCENE_ENABLED = 1 << 1,
	MESH_INSTANCE_FLAG_REQUEST_DESTROY = 1 << 2,
};

enum MeshInstanceUpdateFlag {
	MESH_INSTANCE_UPDATE_NONE = 0,
	MESH_INSTANCE_UPDATE_WORLD_MATRIX = 1 << 0,
};

enum SubMeshInstanceUpdateFlag {
	SUB_MESH_INSTANCE_UPDATE_NONE = 0,
	SUB_MESH_INSTANCE_UPDATE_MATERIAL = 1 << 1,
};

enum GpuCullingRootParameters {
	ROOT_PARAM_GPU_CULLING_SCENE_INFO = 0,
	ROOT_PARAM_GPU_VIEW_INFO,
	ROOT_PARAM_GPU_MESH,
	ROOT_PARAM_GPU_MESH_INSTANCE,
	ROOT_PARAM_GPU_INDIRECT_ARGUMENT_OFFSETS,
	ROOT_PARAM_GPU_INDIRECT_ARGUMENTS,
	ROOT_PARAM_GPU_LOD_LEVEL,
	ROOT_PARAM_GPU_CULLING_VIEW_INFO,
	ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO,
	ROOT_PARAM_GPU_CULLING_RESULT,
	ROOT_PARAM_GPU_HIZ,
	ROOT_PARAM_GPU_COUNT
};

enum MultiDrawCullingRootParameters {
	ROOT_PARAM_MULTI_CULLING_SCENE_INFO = 0,
	ROOT_PARAM_MULTI_VIEW_INFO,
	ROOT_PARAM_MULTI_MESH,
	ROOT_PARAM_MULTI_MESH_INSTANCE,
	ROOT_PARAM_MULTI_SUB_MESH_INFO,
	ROOT_PARAM_MULTI_INDIRECT_ARGUMENT_OFFSETS,
	ROOT_PARAM_MULTI_INDIRECT_ARGUMENTS,
	ROOT_PARAM_MULTI_LOD_LEVEL,
	ROOT_PARAM_MULTI_CULLING_VIEW_INFO,
	ROOT_PARAM_MULTI_CULLING_RESULT,
	ROOT_PARAM_MULTI_HIZ,
	ROOT_PARAM_MULTI_COUNT
};

enum GpuComputeLodRootParameters {
	ROOT_PARAM_LOD_SCENE_INFO = 0,
	ROOT_PARAM_LOD_VIEW_INFO,
	ROOT_PARAM_LOD_MESH,
	ROOT_PARAM_LOD_MESH_INSTANCE,
	ROOT_PARAM_LOD_RESULT_LEVEL,
	ROOT_PARAM_LOD_COUNT
};

enum BuildHizRootParameters {
	ROOT_PARAM_HIZ_INFO = 0,
	ROOT_PARAM_HIZ_INPUT_DEPTH,
	ROOT_PARAM_HIZ_OUTPUT_DEPTH,
	ROOT_PARAM_HIZ_COUNT
};

enum DebugMeshletBoundsRootParameters {
	ROOT_PARAM_DEBUG_MESHLET_SCENE_INFO = 0,
	ROOT_PARAM_DEBUG_MESHLET_MESH,
	ROOT_PARAM_DEBUG_MESHLET_MESH_INSTANCE,
	ROOT_PARAM_DEBUG_MESHLET_LOD_LEVEL,
	ROOT_PARAM_DEBUG_MESHLET_INDIRECT_ARGUMENT,
	ROOT_PARAM_DEBUG_MESHLET_COUNT
};

class PipelineStateGroup;
struct ViewInfo;
struct ShaderSet;

struct SceneCullingInfo {
	u32 _meshInstanceCountMax = 0;
};

struct CullingViewInfo {
	u32 _meshletInfoGpuAddress[2] = {};
};

struct HizInfoConstant {
	u32 _inputDepthWidth = 0;
	u32 _inputDepthHeight = 0;
};

struct CullingResult :public gpu::CullingResult {
	f32 getPassFrustumCullingMeshInstancePersentage() const {
		if (_passFrustumCullingMeshInstanceCount == 0 || _testFrustumCullingMeshInstanceCount == 0) {
			return 0.0f;
		}
		return (_passFrustumCullingMeshInstanceCount / static_cast<f32>(_testFrustumCullingMeshInstanceCount)) * 100.0f;
	}

	f32 getPassOcclusionCullingMeshInstancePersentage() const {
		if (_passOcclusionCullingMeshInstanceCount == 0 || _testOcclusionCullingMeshInstanceCount == 0) {
			return 0.0f;
		}
		return (_passOcclusionCullingMeshInstanceCount / static_cast<f32>(_testOcclusionCullingMeshInstanceCount)) * 100.0f;
	}

	f32 getPassFrustumCullingSubMeshInstancePersentage() const {
		if (_passFrustumCullingSubMeshInstanceCount == 0 || _testFrustumCullingSubMeshInstanceCount == 0) {
			return 0.0f;
		}
		return (_passFrustumCullingSubMeshInstanceCount / static_cast<f32>(_testFrustumCullingSubMeshInstanceCount)) * 100.0f;
	}

	f32 getPassOcclusionCullingSubMeshInstancePersentage() const {
		if (_passOcclusionCullingSubMeshInstanceCount == 0 || _testOcclusionCullingSubMeshInstanceCount == 0) {
			return 0.0f;
		}
		return (_passOcclusionCullingSubMeshInstanceCount / static_cast<f32>(_testOcclusionCullingSubMeshInstanceCount)) * 100.0f;
	}

	f32 getPassFrustumCullingMeshletInstancePersentage() const {
		if (_passFrustumCullingMeshletInstanceCount == 0 || _testFrustumCullingMeshletInstanceCount == 0) {
			return 0.0f;
		}
		return (_passFrustumCullingMeshletInstanceCount / static_cast<f32>(_testFrustumCullingMeshletInstanceCount)) * 100.0f;
	}

	f32 getPassBackfaceCullingMeshletInstancePersentage() const {
		if (_passBackfaceCullingMeshletInstanceCount == 0 || _testBackfaceCullingMeshletInstanceCount == 0) {
			return 0.0f;
		}
		return (_passBackfaceCullingMeshletInstanceCount / static_cast<f32>(_testBackfaceCullingMeshletInstanceCount)) * 100.0f;
	}

	f32 getPassOcclusionCullingMeshletInstancePersentage() const {
		if (_passOcclusionCullingMeshletInstanceCount == 0 || _testFrustumCullingMeshletInstanceCount == 0) {
			return 0.0f;
		}
		return (_passOcclusionCullingMeshletInstanceCount / static_cast<f32>(_testFrustumCullingMeshletInstanceCount)) * 100.0f;
	}

	f32 getPassFrustumCullingTrianglePersentage() const {
		if (_passFrustumCullingTriangleCount == 0 || _testFrustumCullingTriangleCount == 0) {
			return 0.0f;
		}
		return (_passFrustumCullingTriangleCount / static_cast<f32>(_testFrustumCullingTriangleCount)) * 100.0f;
	}

	f32 getPassBackfaceCullingTrianglePersentage() const {
		if (_passBackfaceCullingTriangleCount == 0 || _testBackfaceCullingTriangleCount == 0) {
			return 0.0f;
		}
		return (_passBackfaceCullingTriangleCount / static_cast<f32>(_testBackfaceCullingTriangleCount)) * 100.0f;
	}

	f32 getPassOcclusionCullingTrianglePersentage() const {
		if (_passOcclusionCullingTriangleCount == 0 || _testOcclusionCullingTriangleCount == 0) {
			return 0.0f;
		}
		return (_passOcclusionCullingTriangleCount / static_cast<f32>(_testOcclusionCullingTriangleCount)) * 100.0f;
	}

};

class GraphicsView {
public:
	static constexpr u32 INDIRECT_ARGUMENT_COUNT_MAX = 1024 * 256;
	static constexpr u32 HIERACHICAL_DEPTH_COUNT = 8;
	static constexpr u32 BATCHED_SUBMESH_COUNT_MAX = 1024 * 16;

	void initialize(const ViewInfo* viewInfo);
	void terminate();
	void update();

	void setComputeLodResource(CommandList* commandList);
	void setGpuCullingResources(CommandList* commandList);
	void setGpuFrustumCullingResources(CommandList* commandList);
	void setHizResourcesPass0(CommandList* commandList);
	void setHizResourcesPass1(CommandList* commandList);
	void resourceBarriersComputeLodToUAV(CommandList* commandList);
	void resetResourceComputeLodBarriers(CommandList* commandList);
	void resourceBarriersGpuCullingToUAV(CommandList* commandList);
	void resourceBarriersHizSrvToUav(CommandList* commandList);
	void resourceBarriersHizUavtoSrv(CommandList* commandList);
	void resetResourceGpuCullingBarriers(CommandList* commandList);
	void resetCountBuffers(CommandList* commandList);
	void resetResultBuffers(CommandList* commandList);
	void readbackCullingResultBuffer(CommandList* commandList);

	void setDrawResultDescriptorTable(CommandList* commandList);
	void setDrawCurrentLodDescriptorTable(CommandList* commandList);
	void render(CommandList* commandList, CommandSignature* commandSignature, u32 commandCountMax, u32 indirectArgumentOffset, u32 countBufferOffset);

	DescriptorHandle getCurrentLodLevelSrv() const { return _currentLodLevelSrv; }
	ResourceDesc getHizTextureResourceDesc(u32 level) const;
	const CullingResult* getCullingResult() const;

private:
	GpuBuffer _currentLodLevelBuffer;
	GpuBuffer _indirectArgumentBuffer;
	GpuBuffer _countBuffer;
	GpuBuffer _cullingResultBuffer;
	GpuBuffer _cullingResultReadbackBuffer[BACK_BUFFER_COUNT] = {};
	GpuBuffer _batchedSubMeshInfoBuffer;
	GpuBuffer _cullingViewConstantBuffer;
	GpuBuffer _hizInfoConstantBuffer[2];
	GpuTexture _hizDepthTextures[HIERACHICAL_DEPTH_COUNT] = {};
	DescriptorHandle _hizDepthTextureSrv;
	DescriptorHandle _hizDepthTextureUav;
	DescriptorHandle _hizInfoConstantCbv[2];

	DescriptorHandle _indirectArgumentUavHandle;
	DescriptorHandle _cullingViewInfoCbvHandle;
	DescriptorHandle _cullingResultUavHandle;
	DescriptorHandle _cullingResultCpuUavHandle;
	DescriptorHandle _currentLodLevelUav;
	DescriptorHandle _currentLodLevelSrv;
	DescriptorHandle _countCpuUavHandle;
	gpu::CullingResult* _cullingResultMapPtr[BACK_BUFFER_COUNT] = {};
	gpu::CullingResult* _currentFrameCullingResultMapPtr = nullptr;
	const ViewInfo* _viewInfo = nullptr;

	DescriptorHandle _batchedSubMeshInfoSrv;
	DescriptorHandle _packedMeshletCountUav;
	DescriptorHandle _packedMeshletCountCpuUav;
	GpuBuffer _packedMeshletCountBuffer;
};

class SubMeshInstanceImpl : public SubMeshInstance {
public:
	virtual void setMaterial(Material* material) override;

	// 参照カウンタを操作しない初期化専用のマテリアルセット
	void setDefaultMaterial(Material* material);

	void setUpdateFlags(u8* updateFlags) {
		_updateFlags = updateFlags;
	}
};

class MeshInstanceImpl : public MeshInstance {
public:
	void setEnabled() {
		*_stateFlags |= MESH_INSTANCE_FLAG_SCENE_ENABLED;
	}

	void setStateFlags(u8* stateFlags) {
		_stateFlags = stateFlags;
	}

	void setUpdateFlags(u8* updateFlags) {
		_updateFlags = updateFlags;
	}

	void setMesh(const Mesh* mesh) {
		_mesh = mesh;
	}

	void setGpuMeshInstance(gpu::MeshInstance* meshInstance) {
		_gpuMeshInstance = meshInstance;
	}

	void setGpuLodMeshInstances(gpu::LodMeshInstance* lodMeshInstances) {
		_gpuLodMeshinstances = lodMeshInstances;
	}

	void setGpuSubMeshInstances(gpu::SubMeshInstance* subMeshInstances) {
		_gpuSubMeshInstances = subMeshInstances;
	}

	void setSubMeshInstance(SubMeshInstance* subMeshInstances) {
		_subMeshInstances = subMeshInstances;
	}
};

class Scene {
public:
	static constexpr u32 MESH_INSTANCE_COUNT_MAX = 1024 * 4;
	static constexpr u32 LOD_MESH_INSTANCE_COUNT_MAX = 1024 * 16;
	static constexpr u32 SUB_MESH_INSTANCE_COUNT_MAX = 1024 * 64;
	static constexpr u32 MESHLET_INSTANCE_COUNT_MAX = 1024 * 256;
	static constexpr u32 PACKED_SUB_MESH_COUNT_MAX = 64;

	void initialize();
	void update();
	void processDeletion();
	void terminate();
	void updateMeshInstanceBounds(u32 meshInstanceIndex);
	void deleteMeshInstance(u32 meshInstanceIndex);
	void debugDrawMeshletBounds();
	void debugDrawMeshInstanceBounds();
	void debugDrawGui();

	MeshInstanceImpl* getMeshInstance(u32 index) { return &_meshInstances[index]; }
	MeshInstance* createMeshInstance(const Mesh* mesh);
	DescriptorHandle getMeshInstanceHandles() const { return _meshInstanceHandles; }
	u32 getMeshInstanceCountMax() const { return MESH_INSTANCE_COUNT_MAX; }
	u32 getSubMeshInstanceRefCount(const PipelineStateGroup* pipelineState);
	VramShaderSetSystem* getVramShaderSetSystem() { return &_vramShaderSetSystem; }
	u32 getMeshInstanceCount() const { return _gpuMeshInstances.getInstanceCount(); }
	DescriptorHandle getPackedMeshletOffsetHandles() const { return _packedMeshletOffsetHandle; }

private:
	VramShaderSetSystem _vramShaderSetSystem;
	u8 _meshInstanceStateFlags[MESH_INSTANCE_COUNT_MAX] = {};
	u8 _meshInstanceUpdateFlags[MESH_INSTANCE_COUNT_MAX] = {};
	u8 _subMeshInstanceUpdateFlags[SUB_MESH_INSTANCE_COUNT_MAX] = {};
	MeshInstanceImpl _meshInstances[MESH_INSTANCE_COUNT_MAX] = {};
	SubMeshInstanceImpl _subMeshInstances[SUB_MESH_INSTANCE_COUNT_MAX] = {};

	MultiDynamicQueue<gpu::MeshInstance> _gpuMeshInstances;
	MultiDynamicQueue<gpu::LodMeshInstance> _gpuLodMeshInstances;
	MultiDynamicQueue<gpu::SubMeshInstance> _gpuSubMeshInstances;

	GpuBuffer _meshInstanceBuffer;
	GpuBuffer _lodMeshInstanceBuffer;
	GpuBuffer _subMeshInstanceBuffer;

	DescriptorHandle _meshInstanceHandles;
	Material* _defaultMaterial = nullptr;
	ShaderSet* _defaultShaderSet = nullptr;

	DescriptorHandle _packedMeshletOffsetHandle;
	GpuBuffer _packedMeshletOffsetBuffer;
};