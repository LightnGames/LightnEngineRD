#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>
#include <MeshRenderer/MeshRendererSystem.h>
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
	f32 _nearClip = 0.0f;
	f32 _farClip = 0.0f;
	u32 _inputBitDepth = 0;
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
	// インスタンシング用 0~31　単品用 31~63 
	static constexpr u32 INDIRECT_ARGUMENT_COUNTER_COUNT = gpu::SHADER_SET_COUNT_MAX * 2;
	static constexpr u32 INDIRECT_ARGUMENT_COUNT_MAX = 1024 * 256;
	static constexpr u32 MESHLET_INSTANCE_COUNT_MAX = 1024 * 256;

	void initialize(const ViewInfo* viewInfo);
	void terminate();
	void update();

	void setComputeLodResource(CommandList* commandList);
	void setGpuCullingResources(CommandList* commandList);
	void setHizResourcesPass0(CommandList* commandList);
	void setHizResourcesPass1(CommandList* commandList);
	void resourceBarriersComputeLodToUAV(CommandList* commandList);
	void resetResourceComputeLodBarriers(CommandList* commandList);
	void resourceBarriersGpuCullingToUAV(CommandList* commandList);
	void resourceBarriersHizTextureToUav(CommandList* commandList, u32 offset);
	void resourceBarriersHizUavtoSrv(CommandList* commandList, u32 offset);
	void resourceBarriersHizSrvToTexture(CommandList* commandList);
	void resourceBarriersHizTextureToSrv(CommandList* commandList);
	void resetResourceGpuCullingBarriers(CommandList* commandList);
	void resourceBarriersBuildIndirectArgument(CommandList* commandList);
	void resourceBarriersResetBuildIndirectArgument(CommandList* commandList);
	void resetIndirectArgumentCountBuffers(CommandList* commandList);
	void resetMeshletInstanceInfoCountBuffers(CommandList* commandList);
	void resetResultBuffers(CommandList* commandList);
	void readbackCullingResultBuffer(CommandList* commandList);

	void setDrawResultDescriptorTable(CommandList* commandList);
	void setDrawCurrentLodDescriptorTable(CommandList* commandList);
	void render(CommandList* commandList, CommandSignature* commandSignature, u32 commandCountMax, u32 indirectArgumentOffset, u32 countBufferOffset);

	DescriptorHandle getIndirectArgumentUav() const { return _indirectArgumentUavHandle; }
	DescriptorHandle getMeshletInstanceCountUav() const { return _meshletInstanceInfoCountUav; }
	DescriptorHandle getMeshletInstanceCountSrv() const { return _meshletInstanceInfoCountSrv; }
	DescriptorHandle getMeshletInstanceInfoSrv() const { return _meshletInstanceInfoSrv; }
	DescriptorHandle getMeshletInstanceInfoUav() const { return _meshletInstanceInfoUav; }
	DescriptorHandle getCurrentLodLevelSrv() const { return _currentLodLevelSrv; }
	ResourceDesc getHizTextureResourceDesc(u32 level) const;
	const CullingResult* getCullingResult() const;

private:
	GpuBuffer _currentLodLevelBuffer;
	GpuBuffer _indirectArgumentBuffer;
	GpuBuffer _countBuffer;
	GpuBuffer _cullingResultBuffer;
	GpuBuffer _cullingResultReadbackBuffer;
	GpuBuffer _meshletInstanceInfoBuffer;
	GpuBuffer _meshletInstanceInfoCountBuffer;
	GpuBuffer _primitiveInstancingInfoBuffer;
	GpuBuffer _primitiveInstancingCountBuffer;
	GpuBuffer _cullingViewConstantBuffer;
	GpuBuffer _hizInfoConstantBuffer[2];
	GpuTexture _hizDepthTextures[gpu::HIERACHICAL_DEPTH_COUNT] = {};
	DescriptorHandle _hizDepthTextureSrv;
	DescriptorHandle _hizDepthTextureUav;
	DescriptorHandle _hizInfoConstantCbv[2];

	DescriptorHandle _primitiveInstancingCountCpuUav;
	DescriptorHandle _primitiveInstancingCountUav;
	DescriptorHandle _meshletInstanceInfoCountCpuUav;
	DescriptorHandle _meshletInstanceInfoCountUav;
	DescriptorHandle _meshletInstanceInfoCountSrv;
	DescriptorHandle _meshletInstanceInfoSrv;
	DescriptorHandle _meshletInstanceInfoUav;
	DescriptorHandle _indirectArgumentUavHandle;
	DescriptorHandle _cullingViewInfoCbvHandle;
	DescriptorHandle _cullingResultUavHandle;
	DescriptorHandle _cullingResultCpuUavHandle;
	DescriptorHandle _currentLodLevelUav;
	DescriptorHandle _currentLodLevelSrv;
	DescriptorHandle _countCpuUavHandle;
	gpu::CullingResult _currentFrameCullingResultMapPtr;
	const ViewInfo* _viewInfo = nullptr;
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
	static constexpr u32 PRIMITIVE_INSTANCING_PRIMITIVE_COUNT_MAX = 64;
	static constexpr u32 MESHLET_INSTANCE_MESHLET_COUNT_MAX = 64;
	static constexpr u32 MESHLET_INSTANCE_INFO_COUNT_MAX = (MESHLET_INSTANCE_MESHLET_COUNT_MAX + 1) * gpu::SHADER_SET_COUNT_MAX;
	static constexpr u32 PRIMITIVE_INSTANCING_INFO_COUNT_MAX = PRIMITIVE_INSTANCING_PRIMITIVE_COUNT_MAX * gpu::SHADER_SET_COUNT_MAX;

	void initialize();
	void update();
	void processDeletion();
	void terminate();
	void terminateDefaultResources();
	void updateMeshInstanceBounds(u32 meshInstanceIndex);
	void deleteMeshInstance(u32 meshInstanceIndex);
	void debugDrawMeshletBounds();
	void debugDrawMeshInstanceBounds();
	void debugDrawGui();

	MeshInstanceImpl* getMeshInstance(u32 index) { return &_meshInstances[index]; }
	MeshInstance* createMeshInstance(const Mesh* mesh);
	DescriptorHandle getMeshletInstanceOffsetSrv() const { return _meshletInstanceInfoOffsetSrv; }
	DescriptorHandle getMeshInstanceHandles() const { return _meshInstanceHandles; }
	DescriptorHandle getIndirectArgumentOffsetSrv() const { return _indirectArgumentOffsetSrv; }
	DescriptorHandle getSceneCbv() const { return _cullingSceneConstantHandle; }
	u32 getMeshInstanceCountMax() const { return MESH_INSTANCE_COUNT_MAX; }
	u32 getMeshInstanceCount() const { return _gpuMeshInstances.getInstanceCount(); }
	const u32* getIndirectArgumentInstancingCounts() const { return _indirectArgumentInstancingCounts; }
	const u32* getIndirectArgumentCounts() const { return _indirectArgumentCounts; }
	const u32* getIndirectArgumentOffsets() const { return _indirectArgumentOffsets; }

#if ENABLE_MULTI_INDIRECT_DRAW
	const u32* getMultiDrawIndirectArgumentCounts() const { return _multiDrawIndirectArgumentCounts; }
	const u32* getMultiDrawIndirectArgumentOffsets() const { return _multiDrawIndirectArgumentOffsets; }
	DescriptorHandle getMultiDrawIndirectArgumentOffsetSrv() const { return _multiDrawIndirectArgumentOffsetSrv; }
#endif

private:
	u8 _meshInstanceStateFlags[MESH_INSTANCE_COUNT_MAX] = {};
	u8 _meshInstanceUpdateFlags[MESH_INSTANCE_COUNT_MAX] = {};
	u8 _subMeshInstanceUpdateFlags[SUB_MESH_INSTANCE_COUNT_MAX] = {};
	MeshInstanceImpl _meshInstances[MESH_INSTANCE_COUNT_MAX] = {};
	SubMeshInstanceImpl _subMeshInstances[SUB_MESH_INSTANCE_COUNT_MAX] = {};
	u32 _indirectArgumentOffsets[gpu::SHADER_SET_COUNT_MAX] = {};
	u32 _indirectArgumentCounts[gpu::SHADER_SET_COUNT_MAX] = {};
	u32 _indirectArgumentInstancingCounts[gpu::SHADER_SET_COUNT_MAX] = {};
	u32 _indirectArgumentPrimitiveInstancingCounts[gpu::SHADER_SET_COUNT_MAX] = {};

	MultiDynamicQueue<gpu::MeshInstance> _gpuMeshInstances;
	MultiDynamicQueue<gpu::LodMeshInstance> _gpuLodMeshInstances;
	MultiDynamicQueue<gpu::SubMeshInstance> _gpuSubMeshInstances;

	GpuBuffer _meshInstanceBuffer;
	GpuBuffer _lodMeshInstanceBuffer;
	GpuBuffer _subMeshInstanceBuffer;
	GpuBuffer _meshletInstanceInfoOffsetBuffer;
	GpuBuffer _primitiveInstancingOffsetBuffer;
	GpuBuffer _indirectArgumentOffsetBuffer;
	GpuBuffer _sceneCullingConstantBuffer;

	DescriptorHandle _cullingSceneConstantHandle;
	DescriptorHandle _indirectArgumentOffsetSrv;
	DescriptorHandle _meshletInstanceInfoOffsetSrv;
	DescriptorHandle _meshInstanceHandles;
	Material* _defaultMaterial = nullptr;
	ShaderSet* _defaultShaderSet = nullptr;


#if ENABLE_MULTI_INDIRECT_DRAW
	u32 _multiDrawIndirectArgumentOffsets[gpu::SHADER_SET_COUNT_MAX] = {};
	u32 _multiDrawIndirectArgumentCounts[gpu::SHADER_SET_COUNT_MAX] = {};
	GpuBuffer _multiDrawIndirectArgumentOffsetBuffer;
	DescriptorHandle _multiDrawIndirectArgumentOffsetSrv;
#endif
};