#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
class GpuBuffer;
class GpuMaterialManager {
public:
	void initialize();
	void terminate();
	void update();

	rhi::RootSignature* getDefaultRootSignatures() { return _defaultRootSignatures; }
	rhi::PipelineState* getDefaultPipelineStates() { return _defaultPipelineStates; }
	rhi::GpuDescriptorHandle getParameterGpuSrv(u32 index) { return _parameterSrv.get(index)._gpuHandle; }

	static GpuMaterialManager* Get();

private:
	rhi::RootSignature* _defaultRootSignatures = nullptr;
	rhi::PipelineState* _defaultPipelineStates = nullptr;

	GpuBuffer* _parameterGpuBuffers = nullptr;
	DescriptorHandles _parameterSrv;
};
}