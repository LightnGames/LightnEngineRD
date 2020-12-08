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

struct ClassicShaderSet {
	PipelineState* _debugPipelineState = nullptr;
	PipelineState* _depthPipelineState = nullptr;
	PipelineState* _pipelineState = nullptr;
	RootSignature* _rootSignature = nullptr;
#if ENABLE_MULTI_INDIRECT_DRAW
	CommandSignature* _multiDrawCommandSignature;
#endif
};

struct TempShaderParam {
	Color4 _color;
	Texture* _albedoTexture = nullptr;
};

struct TempShaderParamGpu {
	Color4 _color;
	u32 _albedoTextureIndex;
};

struct ShaderSetImpl :public ShaderSet {
	static constexpr u32 MATERIAL_COUNT_MAX = 128;
	virtual void requestToDelete() override;
	virtual void setTexture(Texture* texture, u64 parameterNameHash) override;

	void initialize(const ShaderSetDesc& desc);
	void terminate();
	void setStateFlags(u8* flags) { _stateFlags = flags; }
	void setUpdateFlags(u8* flags) { _updateFlags = flags; }

	PipelineStateGroup* getPipelineStateGroup() { return _pipelineStateGroup; }
	PipelineStateGroup* getDepthPipelineStateGroup() { return _depthPipelineStateGroup; }
	PipelineStateGroup* getDebugMeshletPipelineStateGroup() { return _debugMeshletPipelineStateGroup; }
	PipelineStateGroup* getDebugLodLevelPipelineStateGroup() { return _debugLodLevelPipelineStateGroup; }
	PipelineStateGroup* getDebugOcclusionPipelineStateGroup() { return _debugOcclusionPipelineStateGroup; }
	PipelineStateGroup* getDebugCullingPassPipelineStateGroup() { return _debugCullingPassPipelineStateGroup; }
	PipelineStateGroup* getDebugDepthPipelineStateGroup() { return _debugDepthPipelineStateGroup; }
	PipelineStateGroup* getDebugTexcoordsPipelineStateGroup() { return _debugTexcoordsPipelineStateGroup; }
	const PipelineStateGroup* getPipelineStateGroup() const { return _pipelineStateGroup; }
	ClassicShaderSet* getClassicShaderSet() { return &_classicShaderSet; }

	u8 _shaderParamStateFlags[MATERIAL_COUNT_MAX] = {};
	DynamicQueue<TempShaderParam> _shaderParams;
private:
	PipelineStateGroup* _pipelineStateGroup = nullptr;
	PipelineStateGroup* _depthPipelineStateGroup = nullptr;
	PipelineStateGroup* _debugCullingPassPipelineStateGroup = nullptr;
	PipelineStateGroup* _debugOcclusionPipelineStateGroup = nullptr;
	PipelineStateGroup* _debugMeshletPipelineStateGroup = nullptr;
	PipelineStateGroup* _debugLodLevelPipelineStateGroup = nullptr;
	PipelineStateGroup* _debugDepthPipelineStateGroup = nullptr;
	PipelineStateGroup* _debugTexcoordsPipelineStateGroup = nullptr;
	ClassicShaderSet _classicShaderSet;
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

class LTN_MATERIAL_SYSTEM_API MaterialSystemImpl :public MaterialSystem {
public:
	static constexpr u32 MATERIAL_COUNT_MAX = 256;
	static constexpr u32 SHADER_SET_COUNT_MAX = 32;
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
};