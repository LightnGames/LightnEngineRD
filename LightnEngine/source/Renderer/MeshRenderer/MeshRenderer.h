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
		INDIRECT_ARGUMENT_OFFSETS,
		INDIRECT_ARGUMENTS,
		LOD_LEVEL,
		CULLING_RESULT,
		HIZ,
		COUNT
	};
};

class MeshRenderer {
public:
	static constexpr u32 INDIRECT_ARGUMENT_CAPACITY = 1024 * 64;

	struct CullingDesc {
		u32 _meshInstanceReserveCount = 0;
		rhi::GpuDescriptorHandle _sceneCbv;
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::GpuDescriptorHandle _meshSrv;
		rhi::GpuDescriptorHandle _meshInstanceSrv;
		rhi::GpuDescriptorHandle _indirectArgumentOffsetSrv;
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
};
}