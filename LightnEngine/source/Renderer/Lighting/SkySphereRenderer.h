#pragma once
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
class GpuTexture;
struct SkySphereRootParam {
	enum {
		VIEW_INFO,
		CUBE_MAP,
		COUNT
	};
};

namespace gpu {
struct SkySphere {
	Float3 _environmentColor;
	f32 _diffuseScale;
	f32 _specularScale;
	u32 _brdfLutTextureIndex;
};
}

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
	void update();

	void render(const RenderDesc& desc);

	static SkySphereRenderer* Get();

	rhi::GpuDescriptorHandle getSkySphereGpuCbv() const { return _skySphereCbv._firstHandle._gpuHandle; }

private:
	rhi::RootSignature _rootSignature;
	rhi::PipelineState _pipelineState;
	GpuBuffer _skySphereConstantGpuBuffer;
	DescriptorHandles _skySphereCbv;
};
}