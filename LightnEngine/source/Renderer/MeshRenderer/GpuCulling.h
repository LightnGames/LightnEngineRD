#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
namespace gpu {
struct CullingInfo {
	u32 _meshInstanceReserveCount;
};
}

struct IndirectArgumentResource;
struct GpuCullingRootParam {
	enum {
		CULLING_INFO = 0,
		VIEW_INFO,
		MESH,
		MESH_INSTANCE,
		SUB_MESH_INFO,
		INDIRECT_ARGUMENT_OFFSET,
		INDIRECT_ARGUMENTS,
		GEOMETRY_GLOBA_OFFSET,
		MESH_INSTANCE_LOD_LEVEL,
		MESH_LOD_STREAM_RANGE,
		CULLING_RESULT,
		//HIZ,
		COUNT
	};
};

class GpuCulling {
public:
	struct CullingDesc {
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::GpuDescriptorHandle _cullingResultUav;
		rhi::CommandList* _commandList = nullptr;
		IndirectArgumentResource* _indirectArgumentResource = nullptr;
	};

	void initialize();
	void terminate();

	void update();
	void gpuCulling(const CullingDesc& desc);

	rhi::GpuDescriptorHandle getCullingInfoCbv() const { return _cullingInfoCbv._gpuHandle; }

	static GpuCulling* Get();

private:
	rhi::RootSignature _gpuCullingRootSignature;
	rhi::PipelineState _gpuCullingPipelineState;

	// TODO: GPUカリング専用のリソースではない
	GpuBuffer _cullingInfoGpuBuffer;
	DescriptorHandle _cullingInfoCbv;
};
}