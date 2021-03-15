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

class IndirectArgumentResource {
public:
	static constexpr u32 INDIRECT_ARGUMENT_COUNTER_COUNT = gpu::SHADER_SET_COUNT_MAX * 2;
	static constexpr u32 INDIRECT_ARGUMENT_COUNT_MAX = 1024 * 256;
	static constexpr u32 MESHLET_INSTANCE_COUNT_MAX = 1024 * 256;

	struct InitializeDesc {
		u32 _indirectArgumentCount = 0;
		u32 _indirectArgumentCounterCount = 0;
	};

	void initialize(const InitializeDesc& desc);
	void terminate();
	void update();

	void resourceBarriersGpuCullingToUAV(CommandList* commandList);
	void resetResourceGpuCullingBarriers(CommandList* commandList);
	void resourceBarriersBuildIndirectArgument(CommandList* commandList);
	void resourceBarriersResetBuildIndirectArgument(CommandList* commandList);
	void resetIndirectArgumentCountBuffers(CommandList* commandList);
	void executeIndirect(CommandList* commandList, CommandSignature* commandSignature, u32 commandCountMax, u32 indirectArgumentOffset, u32 countBufferOffset);

	GpuBuffer* getIndirectArgumentBuffer() { return &_indirectArgumentBuffer; }
	GpuBuffer* getIndirectArgumentCountBuffer() { return &_countBuffer; }
	DescriptorHandle getIndirectArgumentUav() const { return _indirectArgumentUavHandle; }

private:
	GpuBuffer _indirectArgumentBuffer;
	GpuBuffer _countBuffer;

	DescriptorHandle _indirectArgumentUavHandle;
	DescriptorHandle _countCpuUav;
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

class GpuCullingResource {
public:
	void initialize();
	void terminate();
	void update(const ViewInfo* viewInfo);

	DescriptorHandle getCurrentLodLevelSrv() const { return _currentLodLevelSrv; }
	ResourceDesc getHizTextureResourceDesc(u32 level) const;
	const CullingResult* getCullingResult() const;

	void setComputeLodResource(CommandList* commandList);
	void setGpuCullingResources(CommandList* commandList);
	void setHizResourcesPass0(CommandList* commandList);
	void setHizResourcesPass1(CommandList* commandList);
	void resourceBarriersComputeLodToUAV(CommandList* commandList);
	void resetResourceComputeLodBarriers(CommandList* commandList);
	void resourceBarriersHizTextureToUav(CommandList* commandList, u32 offset);
	void resourceBarriersHizUavtoSrv(CommandList* commandList, u32 offset);
	void resourceBarriersHizSrvToTexture(CommandList* commandList);
	void resourceBarriersHizTextureToSrv(CommandList* commandList);
	void resetResultBuffers(CommandList* commandList);
	void readbackCullingResultBuffer(CommandList* commandList);
	void setDrawResultDescriptorTable(CommandList* commandList);
	void setDrawCurrentLodDescriptorTable(CommandList* commandList);

private:
	GpuBuffer _currentLodLevelBuffer;
	GpuBuffer _cullingResultBuffer;
	GpuBuffer _cullingResultReadbackBuffer;
	GpuBuffer _hizInfoConstantBuffer[2];

	GpuTexture _hizDepthTextures[gpu::HIERACHICAL_DEPTH_COUNT] = {};

	gpu::CullingResult _currentFrameCullingResultMapPtr;
	DescriptorHandle _hizDepthTextureSrv;
	DescriptorHandle _hizDepthTextureUav;
	DescriptorHandle _hizInfoConstantCbv[2];
	DescriptorHandle _cullingResultUavHandle;
	DescriptorHandle _cullingResultCpuUavHandle;
	DescriptorHandle _currentLodLevelUav;
	DescriptorHandle _currentLodLevelSrv;
};

class InstancingResource {
public:
	static constexpr u32 INSTANCING_PER_SHADER_COUNT_MAX = 1024 * 2; // SUB_MESH_COUNT_MAX
	static constexpr u32 INDIRECT_ARGUMENT_COUNT_MAX = INSTANCING_PER_SHADER_COUNT_MAX * gpu::SHADER_SET_COUNT_MAX;

	struct UpdateDesc {
		const gpu::SubMesh* _firstSubMesh = nullptr;
		MeshInstanceImpl* _meshInstances = nullptr;
		u32 _countMax = 0;
	};

	void initialize();
	void terminate();
	void update(const UpdateDesc& desc);

	void resetInfoCountBuffers(CommandList* commandList);
	void resourceBarriersGpuCullingToUAV(CommandList* commandList);
	void resetResourceGpuCullingBarriers(CommandList* commandList);

	GpuDescriptorHandle getInfoOffsetSrv() const { return _infoOffsetSrv._gpuHandle; }
	GpuDescriptorHandle getInfoCountUav() const { return _countUav._gpuHandle; }
	GpuDescriptorHandle getInfoCountSrv() const { return _countSrv._gpuHandle; }
	GpuDescriptorHandle getInfoUav() const { return _infoUav._gpuHandle; }
	GpuDescriptorHandle getInfoSrv() const { return _infoSrv._gpuHandle; }

private:
	u32 _infoCounts[INDIRECT_ARGUMENT_COUNT_MAX] = {};
	GpuBuffer _InfoBuffer;
	GpuBuffer _infoCountBuffer;
	GpuBuffer _infoOffsetBuffer;
	DescriptorHandle _infoOffsetSrv;
	DescriptorHandle _countCpuUav;
	DescriptorHandle _countUav;
	DescriptorHandle _countSrv;
	DescriptorHandle _infoUav;
	DescriptorHandle _infoSrv;
};

class MultiDrawInstancingResource {
public:
	struct UpdateDesc {
		MeshInstanceImpl* _meshInstances = nullptr;
		u32 _countMax = 0;
	};

	void initialize();
	void terminate();
	void update(const UpdateDesc& desc);

#if ENABLE_MULTI_INDIRECT_DRAW
	const u32* getIndirectArgumentCounts() const { return _indirectArgumentCounts; }
	const u32* getIndirectArgumentOffsets() const { return _indirectArgumentOffsets; }
	GpuDescriptorHandle getIndirectArgumentOffsetSrv() const { return _indirectArgumentOffsetSrv._gpuHandle; }
#endif
private:
#if ENABLE_MULTI_INDIRECT_DRAW
	u32 _indirectArgumentOffsets[gpu::SHADER_SET_COUNT_MAX] = {};
	u32 _indirectArgumentCounts[gpu::SHADER_SET_COUNT_MAX] = {};
	GpuBuffer _indirectArgumentOffsetBuffer;
	DescriptorHandle _indirectArgumentOffsetSrv;
#endif
};

class Scene {
public:
	static constexpr u32 MESH_INSTANCE_COUNT_MAX = 1024 * 4;
	static constexpr u32 LOD_MESH_INSTANCE_COUNT_MAX = 1024 * 16;
	static constexpr u32 SUB_MESH_INSTANCE_COUNT_MAX = 1024 * 64;
	static constexpr u32 MESHLET_INSTANCE_MESHLET_COUNT_MAX = 64;
	static constexpr u32 MESHLET_INSTANCE_INFO_COUNT_MAX = (MESHLET_INSTANCE_MESHLET_COUNT_MAX + 1) * gpu::SHADER_SET_COUNT_MAX;

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

	bool isUpdatedInstancingOffset() const { return _isUpdatedInstancingOffset; }
	MeshInstanceImpl* getMeshInstance(u32 index) { return &_meshInstances[index]; }
	MeshInstance* createMeshInstance(const Mesh* mesh);
	DescriptorHandle getMeshInstanceHandles() const { return _meshInstanceSrv; }
	DescriptorHandle getSceneCbv() const { return _cullingSceneConstantHandle; }
	u32 getMeshInstanceCountMax() const { return MESH_INSTANCE_COUNT_MAX; }
	u32 getMeshInstanceCount() const { return _gpuMeshInstances.getInstanceCount(); }
	u32 getMeshInstanceArrayCountMax() const { return _gpuMeshInstances.getArrayCountMax(); }

private:
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
	GpuBuffer _sceneCullingConstantBuffer;

	DescriptorHandle _cullingSceneConstantHandle;
	DescriptorHandle _meshInstanceSrv;
	Material* _defaultMaterial = nullptr;
	ShaderSet* _defaultShaderSet = nullptr;
	bool _isUpdatedInstancingOffset = false;
};