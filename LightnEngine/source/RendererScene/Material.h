#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"

namespace ltn {
class Shader;
class PipelineSet {
public:
	const Shader* getVertexShader() const { return _vertexShader; }
	const Shader* getPixelShader() const { return _pixelShader; }

	u32 getParameterSizeInByte() const;
	u16 findMaterialParameterOffset(u32 parameterNameHash) const;
	u16 findMaterialParameters(u8 parameterType, u16* outParameterTypes) const;

	const Shader* _vertexShader = nullptr;
	const Shader* _pixelShader = nullptr;
};

struct MaterialParameterSet {
	u8* _parameters = nullptr;
	u32 _sizeInByte = 0;
	VirtualArray::AllocationInfo _allocationInfo;
};

class Material {
public:
	const PipelineSet* getPipelineSet() const { return _pipelineSet; }

	void setPipelineSet(const PipelineSet* pipelineSet) { _pipelineSet = pipelineSet; }
	void setParameterSet(const MaterialParameterSet& parameters) { _parameters = parameters; }
	void setAssetPath(const char* assetPath) { _assetPath = assetPath; }
	u8* getParameters() { return _parameters._parameters; }

	template <class T>
	const T* getParameter(u32 offset) const { return reinterpret_cast<T*>(_parameters._parameters + offset); }

	const u8* getParameters() const { return _parameters._parameters; }
	MaterialParameterSet* getParameterSet() { return &_parameters; }
	const MaterialParameterSet* getParameterSet() const { return &_parameters; }
	const char* getAssetPath() const { return _assetPath; }

private:
	const char* _assetPath = nullptr;
	const PipelineSet* _pipelineSet = nullptr;
	MaterialParameterSet _parameters;
};

class MaterialScene {
public:
	static constexpr u32 MATERIAL_CAPACITY = 1024;
	static constexpr u32 MATERIAL_PARAMETER_SIZE_IN_BYTE = 1024 * 16;

	struct MaterialCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	const Material* createMaterial(const MaterialCreatationDesc& desc);
	void createMaterials(const MaterialCreatationDesc* descs, Material const** materials, u32 instanceCount);
	void destroyMaterial(const Material* material);
	void destroyMaterials(Material const** materials, u32 instanceCount);

	const UpdateInfos<Material>* getMaterialCreateInfos() const { return &_materialCreateInfos; }
	const UpdateInfos<Material>* getMaterialDestroyInfos() const { return &_materialDestroyInfos; }

	const u8* getEnabledFlags() const { return _enabledFlags; }
	u32 getMaterialIndex(const Material* material) const { return u32(material - _materials); }
	Material* getMaterial(u32 index) { return &_materials[index]; }
	Material* findMaterial(u64 assetPathHash);

	static MaterialScene* Get();

private:
	VirtualArray _materialAllocations;
	VirtualArray::AllocationInfo* _materialAllocationInfos = nullptr;
	Material* _materials = nullptr;

	char** _assetPaths = nullptr;
	u8* _enabledFlags = nullptr;
	u64* _materialAssetPathHashes = nullptr;
	UpdateInfos<Material> _materialCreateInfos;
	UpdateInfos<Material> _materialDestroyInfos;
	ChunkAllocator _chunkAllocator;
};

class MaterialParameterContainer {
public:
	static constexpr u32 MATERIAL_PARAMETER_SIZE_IN_BYTE = 1024 * 16;

	void initialize();
	void terminate();
	void lateUpdate();

	MaterialParameterSet allocateMaterialParameters(u32 sizeInByte);
	void freeMaterialParameters(const MaterialParameterSet* parameterSet);

	u32 getMaterialParameterIndex(const u8* parameter) const { return u32(parameter - _materialParameters); }
	const UpdateInfos<MaterialParameterSet>* getMaterialParameterUpdateInfos() const { return &_materialParameterUpdateInfos; }

	void postUpdateMaterialParameter(const MaterialParameterSet* parameterSet) {
		_materialParameterUpdateInfos.push(parameterSet);
	}

	static MaterialParameterContainer* Get();

private:
	u8* _materialParameters = nullptr;
	UpdateInfos<MaterialParameterSet> _materialParameterUpdateInfos;
	VirtualArray _materialParameterAllocations;
};

class PipelineSetScene {
public:
	static constexpr u32 PIPELINE_SET_CAPACITY = 64;

	void initialize();
	void terminate();
	void lateUpdate();

	struct CreatationDesc {
		const char* _assetPath = nullptr;
	};

	const PipelineSet* createPipelineSet(const CreatationDesc& desc);
	void destroyPipelineSet(const PipelineSet* pipelineSet);

	u32 getPipelineSetIndex(const PipelineSet* pipelineSet) const { return u32(pipelineSet - _pipelineSets); }
	const PipelineSet* findPipelineSet(u64 pipelineDescHash) const;
	const u8* getEnabledFlags() const { return _enabledFlags; }
	const UpdateInfos<PipelineSet>* getCreateInfos() const { return &_pipelineSetCreateInfos; }
	const UpdateInfos<PipelineSet>* getDestroyInfos() const { return &_pipelineSetDestroyInfos; }

	static PipelineSetScene* Get();

private:
	PipelineSet* _pipelineSets = nullptr;
	VirtualArray _pipelineSetAllocations;
	VirtualArray::AllocationInfo* _pipelineSetAllocationInfos = nullptr;
	u64* _assetPathHashes = nullptr;
	u8* _enabledFlags = nullptr;

	UpdateInfos<PipelineSet> _pipelineSetCreateInfos;
	UpdateInfos<PipelineSet> _pipelineSetDestroyInfos;
	ChunkAllocator _chunkAllocator;
};
}