#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include "RenderSceneUtility.h"

namespace ltn {
class Shader;
class Material {
public:
	const Shader* getVertexShader() const { return _vertexShader; }
	const Shader* getPixelShader() const { return _pixelShader; }

	u32 getParameterSizeInByte() const;

	Shader* _vertexShader = nullptr;
	Shader* _pixelShader = nullptr;
};

class MaterialInstance {
public:
	void setParentMaterial(const Material* material) { _parentMaterial = material; }
	void setMaterialParameters(u8* parameters) { _materialParameters = parameters; }

	void setParameter(u32 nameHash, const void* parameter, u32 sizeInByte);

	template<class T>
	void setParameter(u32 nameHash, const T& parameter){
		setParameter(nameHash, &parameter, sizeof(T));
	}

	const Material* getParentMaterial() const { return _parentMaterial; }
	const u8* getMaterialParameters() const { return _materialParameters; }
	bool isEnabledMaterialParameter() const { return _materialParameters != nullptr; }

private:
	const Material* _parentMaterial = nullptr;
	u8* _materialParameters = nullptr;
};

class MaterialScene {
public:
	static constexpr u32 MATERIAL_CAPACITY = 128;
	static constexpr u32 MATERIAL_INSTANCE_CAPACITY = 1024;
	static constexpr u32 MATERIAL_PARAMETER_SIZE_IN_BYTE = 1024 * 16;

	struct MaterialCreatationDesc {
		const char* _assetPath = nullptr;
	};

	struct MaterialInstanceCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	Material* createMaterial(const MaterialCreatationDesc& desc);
	MaterialInstance* createMaterialInstance(const MaterialInstanceCreatationDesc& desc);
	void destroyMaterial(Material* material);
	void destroyMaterialInstance(MaterialInstance* materialInstance);

	u32 getMaterialIndex(const Material* material) const { return u32(material - _materials); }
	u32 getMaterialInstanceIndex(const MaterialInstance* materialInstance) const { return u32(materialInstance - _materialInstances); }
	u32 getMaterialParameterIndex(const u8* parameter) const { return u32(parameter - _materialParameters); }

	const UpdateInfos<Material>* getMaterialCreateInfos() const { return &_materialCreateInfos; }
	const UpdateInfos<Material>* getMaterialDestroyInfos() const { return &_materialDestroyInfos; }
	const UpdateInfos<MaterialInstance>* getMaterialInstanceUpdateInfos() const { return &_materialInstanceUpdateInfos; }
	const u8* getEnabledFlags() const { return _enabledFlags; }

	UpdateInfos<MaterialInstance>* getMaterialInstanceUpdateInfos() { return &_materialInstanceUpdateInfos; }
	Material* findMaterial(u64 assetPathHash);
	MaterialInstance* findMaterialInstance(u64 assetPathHash);

	static MaterialScene* Get();

private:
	VirtualArray _materialAllocations;
	VirtualArray _materialInstanceAllocations;
	VirtualArray _materialParameterAllocations;
	VirtualArray::AllocationInfo* _materialAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _materialInstanceAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _materialParameterAllocationInfos = nullptr;
	Material* _materials = nullptr;
	MaterialInstance* _materialInstances = nullptr;

	u64* _materialAssetPathHashes = nullptr;
	u64* _materialInstanceAssetPathHashes = nullptr;
	u8* _materialParameters = nullptr;
	u8* _enabledFlags = nullptr;
	UpdateInfos<Material> _materialCreateInfos;
	UpdateInfos<Material> _materialDestroyInfos;
	UpdateInfos<MaterialInstance> _materialInstanceUpdateInfos;
};
}