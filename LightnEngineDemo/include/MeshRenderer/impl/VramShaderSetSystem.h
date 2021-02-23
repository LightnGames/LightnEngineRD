#pragma once
#include <Core/System.h>
#include <GfxCore/impl/GpuResourceImpl.h>
#include <GfxCore/impl/DescriptorHeap.h>
#include <MaterialSystem/MaterialSystem.h>

struct MaterialImpl;

struct MaterialParameter {
	Color4 _baseColor;
	u32 _albedoTextureIndex = 0;
};

struct MaterialMapKey {
	u16 _vramShaderSetIndex = 0;
	u16 _materialInstanceIndex = 0;
};

class VramShaderSet {
public:
	static constexpr u32 MATERIAL_INSTANCE_COUNT_MAX = 256;
	void initialize();
	void terminate();
	void updateMaterialParameter(u32 materialInstanceIndex);
	void removeMaterialInstance(u32 materialInstanceIndex);
	u32 addMaterialInstance(MaterialImpl* material);
	u32 findMaterialInstance(Material* material) const;
	DescriptorHandle getMaterialParametersSrv() const { return _materialParameterSrv; }

private:
	GpuBuffer _parameterBuffer;
	DescriptorHandle _materialParameterSrv;
	DynamicQueue<MaterialImpl*> _materialInstances;
};

class VramShaderSetSystem {
public:
	void initialize();
	void update();
	void processDeletion();
	void terminate();

	u32 getShaderSetIndex(const Material* material);
	VramShaderSet* getShaderSet(u32 index) { return &_shaderSets[index]; }

private:
	VramShaderSet _shaderSets[MaterialSystem::SHADER_SET_COUNT_MAX] = {};
	MaterialMapKey _materialMapKeys[MaterialSystem::MATERIAL_COUNT_MAX] = {};
};
