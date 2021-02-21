#pragma once
#include <Core/System.h>
#include <GfxCore/impl/GpuResourceImpl.h>
#include <GfxCore/impl/DescriptorHeap.h>

struct CommandList;
struct Material;
class PipelineStateGroup;

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
	static constexpr u32 INDIRECT_ARGUMENT_COUNT_MAX = 1024 * 16;
	static constexpr u32 MATERIAL_INSTANCE_COUNT_MAX = 256;
	void initialize();
	void terminate();
	u32 getTotalRefCount() const { return _totalRefCount; }

	GpuBuffer _parameterBuffer;
	DescriptorHandle _materialParameterSrv;
	DynamicQueue<Material*> _materialInstances;
	u32 _materialRefCounts[MATERIAL_INSTANCE_COUNT_MAX] = {};
	u32 _totalRefCount = 0;
};

class VramShaderSetSystem {
public:
	static constexpr u32 SHADER_SET_COUNT_MAX = 32;
	static constexpr u32 MATERIAL_COUNT_MAX = 1024;

	void initialize();
	void update();
	void processDeletion();
	void terminate();

	u32 getMaterialInstanceTotalRefCount(u32 pipelineStateIndex);
	u32 getIndexVramMaterial(const Material* material);
	u32 getShaderSetIndex(const Material* material);
	void addRefCountMaterial(Material* material);
	void removeRefCountMaterial(const Material* material);
	VramShaderSet* getShaderSet(u32 index) { return &_shaderSets[index]; }

private:
	VramShaderSet _shaderSets[SHADER_SET_COUNT_MAX] = {};
	MaterialMapKey _materialMapKeys[MATERIAL_COUNT_MAX] = {};
};