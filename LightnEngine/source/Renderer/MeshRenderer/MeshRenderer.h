#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
namespace gpu {
using IndirectArgument = rhi::DrawIndexedArguments;
constexpr u32 HIERACHICAL_DEPTH_COUNT = 8;

struct CullingInfo {
	u32 _meshInstanceReserveCount;
};

struct IndirectArgumentSubInfo {
	u32 _meshInstanceIndex;
	u32 _materialIndex;
	u32 _materialParameterOffset;
};
}

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
		HIZ,
		COUNT
	};
};

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

class MeshRenderer {
public:
	static constexpr u32 INDIRECT_ARGUMENT_CAPACITY = 1024 * 64;

	struct CullingDesc {
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::GpuDescriptorHandle _cullingResultUav;
		rhi::CommandList* _commandList = nullptr;
	};

	struct ComputeLodDesc {
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::CommandList* _commandList = nullptr;
	};

	struct RenderDesc {
		rhi::CommandList* _commandList = nullptr;
		rhi::RootSignature* _rootSignatures = nullptr;
		rhi::PipelineState* _pipelineStates = nullptr;
		rhi::GpuDescriptorHandle _viewCbv;
		u32 _pipelineStateCount = 0;
		const u8* _enabledFlags = nullptr;
	};

	void initialize();
	void terminate();

	void update();
	void culling(const CullingDesc& desc);
	void computeLod(const ComputeLodDesc& desc);
	void render(const RenderDesc& desc);

	static MeshRenderer* Get();

private:
	GpuBuffer _cullingInfoGpuBuffer;
	GpuBuffer _indirectArgumentGpuBuffer;
	GpuBuffer _indirectArgumentCountGpuBuffer;
	GpuBuffer _indirectArgumentSubInfoGpuBuffer;
	DescriptorHandles _indirectArgumentUav;
	DescriptorHandle _indirectArgumentCountCpuUav;
	DescriptorHandle _cullingInfoCbv;
	DescriptorHandle _indirectArgumentSubInfoSrv;

	rhi::CommandSignature _commandSignature;
	rhi::RootSignature _gpuCullingRootSignature;
	rhi::PipelineState _gpuCullingPipelineState;
	rhi::RootSignature _computeLodRootSignature;
	rhi::PipelineState _computeLodPipelineState;
};
}