#pragma once
#include <MaterialSystem/MaterialSystem.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
struct PipelineState;
struct RootSignature;
struct CommandSignature;

#if ENABLE_DEBUG_OUTPUT_PRIMITIVE
#define DEBUG_ROOT_PARAM_OUTPUT_PRIMITIVE(x) x,
#else
#define DEBUG_ROOT_PARAM_OUTPUT_PRIMITIVE(x)
#endif

enum PipelineStateGrpupFlags {
	PIPELINE_STATE_GROUP_FLAG_NONE = 0,
	PIPELINE_STATE_GROUP_FLAG_REQUEST_DESTROY = 1 << 0,
};

enum DefaultMeshRootParameterIndex {
	ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT = 0,
	ROOT_DEFAULT_MESH_VIEW_CONSTANT,
	ROOT_DEFAULT_MESH_MATERIALS,
	ROOT_DEFAULT_MESH_MESH,
	ROOT_DEFAULT_MESH_MESH_INSTANCE,
	ROOT_DEFAULT_MESH_BATCHED_MESHLET_INFO,
	ROOT_DEFAULT_MESH_PACKED_MESHLET_INFO,
	ROOT_DEFAULT_MESH_VERTEX_RESOURCES,
	ROOT_DEFAULT_MESH_TEXTURES,
	ROOT_DEFAULT_MESH_LOD_LEVEL,
	DEBUG_ROOT_PARAM_OUTPUT_PRIMITIVE(ROOT_DEFAULT_MESH_DEBUG_OUTPUT_PRIMITIVE)
	ROOT_DEFAULT_MESH_CULLING_RESULT,
	ROOT_DEFAULT_MESH_HIZ,
	ROOT_DEFAULT_MESH_COUNT
};

enum ClassicMeshRootParameterIndex {
	ROOT_CLASSIC_MESH_SCENE_CONSTANT = 0,
	ROOT_CLASSIC_MESH_INFO,
	ROOT_CLASSIC_MESH_MATERIALS,
	ROOT_CLASSIC_MESH_MESH_INSTANCE,
	ROOT_CLASSIC_MESH_TEXTURES,
	ROOT_CLASSIC_MESH_COUNT
};

struct PipelineStateGroupDesc {
	ComparisonFunc _depthComparisonFunc;
	BlendDesc _blendDesc;
	const char* _meshShaderFilePath = nullptr;
	const char* _amplificationShaderFilePath = nullptr;
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
	void initialize(const PipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc);
	void terminate();
	void requestToDestroy();
	PipelineState* getPipelineState() { return _pipelineState; }
	RootSignature* getRootSignature() { return _rootSignature; }
	CommandSignature* getCommandSignature() { return _commandSignature; }

	void setStateFlags(u8* flags) { _stateFlags = flags; }

private:
	u8* _stateFlags = nullptr;
	PipelineState* _pipelineState = nullptr;
	RootSignature* _rootSignature = nullptr;
	//ValueDynamicQueue _materialStructs;
	//ShaderInfo _shaderInfo;
	CommandSignature* _commandSignature;
};

class LTN_MATERIAL_SYSTEM_API PipelineStateSystem {
public:
	static constexpr u32 PIPELINE_STATE_GROUP_COUNT_MAX = 16;
	void initialize();
	void update();
	void processDeletion();
	void terminate();

	PipelineStateGroup* getGroup(u32 index) { return &_pipelineStates[index]; }
	u64 getPipelineStateGrpupHash(const PipelineStateGroup* group) const;
	u32 getGroupArrayCount() const { return _pipelineStates.getArrayCountMax(); }
	u32 getGroupIndex(const PipelineStateGroup* pipelineState) const;
	PipelineStateGroup* createPipelineStateGroup(const PipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc);
	const u8* getStateArray() const { return _stateFlags; }

	static PipelineStateSystem* Get();
private:
	DynamicQueue<PipelineStateGroup> _pipelineStates;
	u64 _pipelineStateHashes[PIPELINE_STATE_GROUP_COUNT_MAX] = {};
	u8 _stateFlags[PIPELINE_STATE_GROUP_COUNT_MAX] = {};
	u16 _refCounts[PIPELINE_STATE_GROUP_COUNT_MAX] = {};
};