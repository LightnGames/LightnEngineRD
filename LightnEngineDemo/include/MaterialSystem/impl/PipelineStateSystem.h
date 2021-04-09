#pragma once
#include <MaterialSystem/MaterialSystem.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
struct PipelineState;
struct RootSignature;
struct CommandSignature;

enum PipelineStateGrpupFlags {
	PIPELINE_STATE_GROUP_FLAG_NONE = 0,
	PIPELINE_STATE_GROUP_FLAG_REQUEST_DESTROY = 1 << 0,
};

struct DefaultMeshRootParameter {
	enum {
		CULLING_VIEW_CONSTANT = 0,
		VIEW_CONSTANT,
		MATERIALS,
		MESH,
		MESH_INSTANCE,
		INDIRECT_CONSTANT,
		MESHLET_INFO,
		VERTEX_RESOURCES,
		TEXTURES,
		LOD_LEVEL,
		MESHLET_PRIMITIVE_INFO,
		MESHLET_MESH_INSTANCE_INDEX,
		MESH_INSTANCE_WORLD_MATRIX,
		CULLING_RESULT,
		HIZ,
		ROOT_DEFAULT_MESH_COUNT
	};

};

enum ClassicMeshRootParameterIndex {
	ROOT_CLASSIC_MESH_SCENE_CONSTANT = 0,
	ROOT_CLASSIC_MESH_INFO,
	ROOT_CLASSIC_MESH_MATERIALS,
	ROOT_CLASSIC_MESH_MESH_INSTANCE,
	ROOT_CLASSIC_MESH_TEXTURES,
	ROOT_CLASSIC_MESH_COUNT
};

struct MeshShaderPipelineStateGroupDesc {
	ComparisonFunc _depthComparisonFunc;
	BlendDesc _blendDesc;
	FillMode _fillMode = FILL_MODE_SOLID;
	const char* _meshShaderFilePath = nullptr;
	const char* _amplificationShaderFilePath = nullptr;
	const char* _pixelShaderFilePath = nullptr;
};

struct ClassicPipelineStateGroupDesc {
	ComparisonFunc _depthComparisonFunc;
	BlendDesc _blendDesc;
	FillMode _fillMode = FILL_MODE_SOLID;
	const char* _vertexShaderFilePath = nullptr;
	const char* _pixelShaderFilePath = nullptr;
};

struct ShaderInfo {
	// constant buffers
	// textures
	// shader resource views
};

class PipelineStateGroup {
public:
	static constexpr u32 MATERIAL_STRUCT_COUNT_MAX = 64;
	void initialize(const MeshShaderPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc);
	void initialize(const ClassicPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc);
	void terminate();
	void requestToDestroy();
	PipelineState* getPipelineState() { return _pipelineState; }
	RootSignature* getRootSignature() { return _rootSignature; }

	void setStateFlags(u8* flags) { _stateFlags = flags; }

private:
	u8* _stateFlags = nullptr;
	PipelineState* _pipelineState = nullptr;
	RootSignature* _rootSignature = nullptr;
	//ValueDynamicQueue _materialStructs;
	//ShaderInfo _shaderInfo;
};

class LTN_MATERIAL_SYSTEM_API PipelineStateSystem {
public:
	static constexpr u32 PIPELINE_STATE_GROUP_COUNT_MAX = 32;
	void initialize();
	void update();
	void processDeletion();
	void terminate();

	PipelineStateGroup* getGroup(u32 index) { return &_pipelineStates[index]; }
	u64 getPipelineStateGrpupHash(const PipelineStateGroup* group) const;
	u32 getGroupArrayCount() const { return _pipelineStates.getArrayCountMax(); }
	u32 getGroupIndex(const PipelineStateGroup* pipelineState) const;
	PipelineStateGroup* createPipelineStateGroup(const MeshShaderPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc);
	PipelineStateGroup* createPipelineStateGroup(const ClassicPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc);
	const u8* getStateArray() const { return _stateFlags; }

	static PipelineStateSystem* Get();
private:
	u32 findPipelineStateGroup(u64 hash) const;

private:
	DynamicQueue<PipelineStateGroup> _pipelineStates;
	u64 _pipelineStateHashes[PIPELINE_STATE_GROUP_COUNT_MAX] = {};
	u8 _stateFlags[PIPELINE_STATE_GROUP_COUNT_MAX] = {};
	u16 _refCounts[PIPELINE_STATE_GROUP_COUNT_MAX] = {};
};