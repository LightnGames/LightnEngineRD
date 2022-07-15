#pragma once
#include <Renderer/RHI/Rhi.h>

namespace ltn {
struct ComputeLodLevelRootParam {
	enum {
		CULLING_INFO = 0,
		VIEW_INFO,
		MESH,
		MESH_INSTANCE,
		SUB_MESH_INFO,
		MESH_INSTANCE_LOD_LEVEL,
		MESH_LOD_LEVEL,
		MATERIAL_LOD_LEVEL,
		COUNT
	};
};

class ComputeLod {
public:
	struct ComputeLodDesc {
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::GpuDescriptorHandle _cullingInfoCbv;
		rhi::CommandList* _commandList = nullptr;
	};

	void initialize();
	void terminate();

	void computeLod(const ComputeLodDesc& desc);

	static ComputeLod* Get();

private:
	rhi::RootSignature _computeLodRootSignature;
	rhi::PipelineState _computeLodPipelineState;
};
}