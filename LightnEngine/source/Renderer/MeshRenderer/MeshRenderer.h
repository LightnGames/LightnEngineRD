#pragma once
#include <Renderer/RHI/Rhi.h>

namespace ltn {
class MeshRenderer {
public:
	struct RenderDesc {
		rhi::CommandList* _commandList = nullptr;
	};

	void initialize();
	void terminate();

	void render(const RenderDesc& desc);

	static MeshRenderer* Get();

private:
	rhi::RootSignature _gpuCullingRootSIgnature;
	rhi::PipelineState _gpuCullingPipelineState;
};
}