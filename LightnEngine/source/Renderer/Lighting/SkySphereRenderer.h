#pragma once
#include <Renderer/RHI/Rhi.h>

namespace ltn {
class GpuTexture;
struct SkySphereRootParam {
	enum {
		VIEW_INFO,
		CUBE_MAP,
		COUNT
	};
};
class SkySphereRenderer {
public:
	struct RenderDesc {
		rhi::CommandList* _commandList = nullptr;
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::CpuDescriptorHandle _viewRtv;
		rhi::CpuDescriptorHandle _viewDsv;
		GpuTexture* _viewDepthGpuTexture = nullptr;
	};

	void initialize();
	void terminate();

	void render(const RenderDesc& desc);

	static SkySphereRenderer* Get();

private:
	rhi::RootSignature _rootSignature;
	rhi::PipelineState _pipelineState;
};
}