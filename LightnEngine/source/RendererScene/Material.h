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

	Shader* _vertexShader = nullptr;
	Shader* _pixelShader = nullptr;
};

class MaterialInstance {
public:
};

class MaterialScene {
public:
	static constexpr u32 MATERIAL_COUNT_MAX = 128;
	static constexpr u32 MATERIAL_INSTANCE_COUNT_MAX = 1024;

	struct CreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();
	void lateUpdate();

	Material* createMaterial(const CreatationDesc& desc);
	void destroyMaterial(Material* material);

	u32 getMaterialIndex(const Material* material) const { return u32(material - _materials); }
	const UpdateInfos<Material>* getCreateInfos() const { return &_materialCreateInfos; }
	const UpdateInfos<Material>* getDestroyInfos() const { return &_materialDestroyInfos; }
	const u8* getEnabledFlags() const { return _enabledFlags; }

	static MaterialScene* Get();

private:
	VirtualArray _materialAllocations;
	VirtualArray::AllocationInfo* _materialAllocationInfos = nullptr;
	Material* _materials = nullptr;

	u8* _enabledFlags = nullptr;
	UpdateInfos<Material> _materialCreateInfos;
	UpdateInfos<Material> _materialDestroyInfos;
};
}