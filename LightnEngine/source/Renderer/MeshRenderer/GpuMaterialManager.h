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

	rhi::RootSignature* getRootSignatures() { return _defaultRootSignatures; }
	rhi::PipelineState* getPipelineStates() { return _defaultPipelineStates; }
	DescriptorHandles getParameterDescriptors() { return _parameterDescriptors; }

	static GpuMaterialManager* Get();

private:
	rhi::RootSignature* _defaultRootSignatures = nullptr;
	rhi::PipelineState* _defaultPipelineStates = nullptr;

	GpuBuffer* _parameterGpuBuffers = nullptr;
	DescriptorHandles _parameterDescriptors;
};
}