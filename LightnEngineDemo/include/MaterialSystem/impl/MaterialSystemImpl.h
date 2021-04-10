#pragma once
#include <MaterialSystem/MaterialSystem.h>
#include <MaterialSystem/impl/PipelineStateSystem.h>

enum MaterialStateFlags {
	MATERIAL_STATE_FLAG_NONE = 0,
	MATERIAL_STATE_FLAG_CREATED = 1 << 0,
	MATERIAL_STATE_FLAG_ENABLED = 1 << 1,
	MATERIAL_STATE_FLAG_REQEST_DESTROY = 1 << 2,
};

enum MaterialUpdateFlags {
	MATERIAL_UPDATE_FLAG_NONE = 0,
	MATERIAL_UPDATE_FLAG_UPDATE_PARAMS = 1 << 0,
};

enum ShaderSetStateFlags {
	SHADER_SET_STATE_FLAG_NONE = 0,
	SHADER_SET_STATE_FLAG_CREATED = 1 << 0,
	SHADER_SET_STATE_FLAG_ENABLED = 1 << 1,
	SHADER_SET_STATE_FLAG_REQEST_DESTROY = 1 << 2,
};

enum ShaderSetMaterialStateFlags {
	SHADER_SET_MATERIAL_STATE_NONE = 0,
	SHADER_SET_MATERIAL_STATE_REQUEST_DESTROY = 1 << 0,
};

struct TempShaderParam {
	Color4 _color;
	Texture* _albedoTexture = nullptr;
};

struct TempShaderParamGpu {
	Color4 _color;
	u32 _albedoTextureIndex;
};

struct ShaderSetImplDesc {
	PipelineStateGroup** _primitiveInstancingPipelineStateGroup = nullptr;
	PipelineStateGroup** _primitiveInstancingDepthPipelineStateGroup = nullptr;
	PipelineStateGroup** _pipelineStateGroup = nullptr;
	PipelineStateGroup** _depthPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugCullingPassPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugOcclusionPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugMeshletPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugLodLevelPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugDepthPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugTexcoordsPipelineStateGroup = nullptr;
	PipelineStateGroup** _debugWireFramePipelineStateGroup = nullptr;
	PipelineStateGroup** _classicPipelineStateGroup = nullptr;
	PipelineStateGroup** _classicDepthPipelineStateGroup = nullptr;
	CommandSignature** _commandSignature = nullptr;
	CommandSignature** _msCommandSignature = nullptr;
	CommandSignature** _multiDrawCommandSignature = nullptr;
};

struct ShaderSetImpl :public ShaderSet {
	static constexpr u32 MATERIAL_COUNT_MAX = 128;
	virtual void requestToDelete() override;
	virtual void setTexture(Texture* texture, u64 parameterNameHash) override;

	void initialize(const ShaderSetDesc& desc, ShaderSetImplDesc& implDesc);
	void terminate();
	void setStateFlags(u8* flags) { _stateFlags = flags; }
	void setUpdateFlags(u8* flags) { _updateFlags = flags; }

	u8 _shaderParamStateFlags[MATERIAL_COUNT_MAX] = {};
	DynamicQueue<TempShaderParam> _shaderParams;
};

struct MaterialImpl :public Material {
	static constexpr u32 TEXTURE_COUNT_MAX = 8;
	virtual void requestToDelete() override;
	virtual void setTexture(Texture* texture, u64 parameterNameHash) override;
	ShaderSetImpl* getShaderSet() { return _shaderSet; }
	const ShaderSetImpl* getShaderSet() const { return _shaderSet; }
	TempShaderParam* getShaderParam() { return _shaderParam; }

	void setShaderSetStateFlags(u8* flags) { _shaderSetStateFlags = flags; }
	void setShaderParam(TempShaderParam* param) { _shaderParam = param; }
	void setStateFlags(u8* flags) { _stateFlags = flags; }
	void setUpdateFlags(u8* flags) { _updateFlags = flags; }
	void setShaderSet(ShaderSetImpl* shaderSet) { _shaderSet = shaderSet; }

private:
	u8* _shaderSetStateFlags = nullptr;
	ShaderSetImpl* _shaderSet = nullptr;
	TempShaderParam* _shaderParam = nullptr;
};

class PipelineStateSet {
public:
	void requestDelete(u32 shaderSetIndex);

public:
	PipelineStateGroup* _pipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _depthPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugCullingPassPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugOcclusionPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugMeshletPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugLodLevelPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugDepthPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugTexcoordsPipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	PipelineStateGroup* _debugWireFramePipelineStateGroups[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	CommandSignature* _commandSignatures[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	CommandSignature* _depthCommandSignatures[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
};

class LTN_MATERIAL_SYSTEM_API MaterialSystemImpl :public MaterialSystem {
public:
	enum Type {
		TYPE_MESH_SHADER = 0,
		TYPE_MESH_SHADER_PRIM_INSTANCING,
		TYPE_CLASSIC,
		TYPE_COUNT
	};

	void initialize();
	void update();
	void processDeletion();
	void terminate();

	u32 findShaderSetIndex(u64 fileHash);
	u32 getMaterialCount() const { return _materials.getArrayCountMax(); }
	u32 getShaderSetCount() const { return _shaderSets.getArrayCountMax(); }
	MaterialImpl* getMaterial(u32 index = 0) { return &_materials[index]; }
	ShaderSetImpl* getShaderSet(u32 index = 0) { return &_shaderSets[index]; }
	const u8* getShaderSetStateFlags(u32 index = 0) const { return &_shaderSetStateFlags[index]; }
	const u8* getMaterialStateFlags(u32 index = 0) const { return &_materialStateFlags[index]; }
	const u8* getMaterialUpdateFlags(u32 index = 0) const { return &_materialUpdateFlags[index]; }
	u32 getShaderSetIndex(const ShaderSetImpl* shaderSet) const;
	u32 getMaterialIndex(const Material* material) const;
	bool isEnabledShaderSet(const ShaderSetImpl* shaderSet) const;

	PipelineStateSet* getPipelineStateSet(Type type) { return &_pipelineStateSets[type]; }

	virtual ShaderSet* createShaderSet(const ShaderSetDesc& desc) override;
	virtual Material* createMaterial(const MaterialDesc& desc) override;
	virtual Material* findMaterial(u64 filePathHash) override;
	static MaterialSystemImpl* Get();

private:
	DynamicQueue<MaterialImpl> _materials;
	DynamicQueue<ShaderSetImpl> _shaderSets;
	u64 _materialFileHashes[MATERIAL_COUNT_MAX] = {};
	u64 _shaderSetFileHashes[SHADER_SET_COUNT_MAX] = {};
	u8 _materialStateFlags[MATERIAL_COUNT_MAX] = {};
	u8 _shaderSetStateFlags[SHADER_SET_COUNT_MAX] = {};
	u8 _materialUpdateFlags[MATERIAL_COUNT_MAX] = {};

	PipelineStateSet _pipelineStateSets[TYPE_COUNT] = {};
};