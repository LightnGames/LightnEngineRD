#pragma once
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
namespace ltn {
namespace gpu {
using IndirectArgument = rhi::DrawIndexedArguments;
constexpr u32 HIERACHICAL_DEPTH_COUNT = 8;

struct IndirectArgumentSubInfo {
	u32 _meshInstanceIndex;
	u32 _materialIndex;
	u32 _materialParameterOffset;
	u32 _triangleOffset;
};
}

struct IndirectArgumentResource {
	void initialize();
	void terminate();

	static constexpr u32 INDIRECT_ARGUMENT_CAPACITY = 1024 * 64;
	GpuBuffer _indirectArgumentGpuBuffer;
	GpuBuffer _indirectArgumentCountGpuBuffer;
	GpuBuffer _indirectArgumentSubInfoGpuBuffer;
	DescriptorHandles _indirectArgumentUav;
	DescriptorHandle _indirectArgumentCountCpuUav;
	DescriptorHandle _indirectArgumentSubInfoSrv;
};
}