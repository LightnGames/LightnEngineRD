#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuBuffer.h>

namespace ltn {
namespace gpu {
using IndirectArgument = rhi::DrawIndexedArguments;
}
class MeshRenderer {
public:
	struct CullingDesc {
		rhi::CommandList* _commandList = nullptr;
	};

	struct BuildIndirectArgumentDesc {
		rhi::CommandList* _commandList = nullptr;
	};

	struct RenderDesc {
		rhi::CommandList* _commandList = nullptr;
		rhi::RootSignature* _rootSignatures = nullptr;
		rhi::PipelineState* _pipelineStates = nullptr;
		rhi::GpuDescriptorHandle _viewConstantDescriptor;
		const u8* _enabledFlags = nullptr;
		const u32* _indirectArgumentCounts = nullptr;
		u32 _pipelineStateCount = 0;
	};

	void initialize();
	void terminate();

	void culling(const CullingDesc& desc);
	void buildIndirectArgument(const BuildIndirectArgumentDesc& desc);
	void render(const RenderDesc& desc);

	static MeshRenderer* Get();

private:
	GpuBuffer _indirectArgumentGpuBuffer;
	GpuBuffer _indirectArgumentCountBuffer;

	rhi::CommandSignature _commandSignature;
	rhi::RootSignature _gpuCullingRootSignature;
	rhi::PipelineState _gpuCullingPipelineState;
};
}