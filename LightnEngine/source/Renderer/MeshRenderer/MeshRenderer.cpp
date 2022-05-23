#include "MeshRenderer.h"

namespace ltn {
namespace {
MeshRenderer g_meshRenderer;
}

void MeshRenderer::initialize() {
}

void MeshRenderer::terminate() {
}

void MeshRenderer::culling(const CullingDesc& desc) {
}

void MeshRenderer::buildIndirectArgument(const BuildIndirectArgumentDesc& desc) {
}

void MeshRenderer::render(const RenderDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	for (u32 i = 0; i < desc._pipelineStateCount; ++i) {
		commandList->setPipelineState(&desc._pipelineStates[i]);

		u32 count = desc._indirectArgumentCounts[i];
		u32 offset = sizeof(gpu::IndirectArgument) * i;
		commandList->executeIndirect(&_commandSignature, count, _indirectArgumentGpuBuffer.getResource(), offset, _indirectArgumentCountBuffer.getResource(), 0);
	}
}

MeshRenderer* MeshRenderer::Get() {
	return &g_meshRenderer;
}
}
