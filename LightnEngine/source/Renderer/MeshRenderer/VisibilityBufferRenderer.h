#pragma once
#include <Renderer/RenderCore/GpuTexture.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Core/Type.h>

namespace ltn {
struct IndirectArgumentResource;
struct VisibilityBufferBuildShaderIdRootParam {
enum {
	CONSTANT = 0,
	TRIANGLE_ID,
	SHADER_RANGE,
	COUNT
};
};

struct VisibilityBufferFrameResource {
	void setUpFrameResource(rhi::CommandList* commandList);

	GpuTexture* _triangleIdTexture = nullptr;
	GpuTexture* _triangleShaderIdTexture = nullptr;
	GpuTexture* _shaderIdDepth = nullptr;
	GpuBuffer* _shaderRangeBuffer[2] = {};// [min, max]

	DescriptorHandles _triangleIdRtv;
	DescriptorHandle _triangleIdSrv;
	DescriptorHandle _shaderIdSrv;
	DescriptorHandle _shaderIdDsv;
	DescriptorHandles _shaderRangeSrv;
	DescriptorHandles _shaderRangeUav;
	DescriptorHandles _shaderRangeCpuUav;
};

class VisiblityBufferRenderer {
public:
	static constexpr u32 SHADER_RANGE_TILE_SIZE = 64; // px

	struct BuildShaderIdConstant {
		u32 _resolution[2] = {};
	};

	struct ShadingConstant {
		f32 _quadNdcSize[2] = {};
		u32 _shaderRangeResolution[2] = {};
	};

	struct BuildShaderIdDesc {
		VisibilityBufferFrameResource* _frameResource = nullptr;
		rhi::CommandList* _commandList = nullptr;
	};

	struct ClearShaderIdDesc {
		VisibilityBufferFrameResource* _frameResource = nullptr;
		rhi::CommandList* _commandList = nullptr;
	};

	struct ShadingPassDesc {
		VisibilityBufferFrameResource* _frameResource = nullptr;
		rhi::CommandList* _commandList = nullptr;
		rhi::RootSignature* _rootSignatures = nullptr;
		rhi::PipelineState* _pipelineStates = nullptr;
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::GpuDescriptorHandle _viewDepthSrv;
		rhi::CpuDescriptorHandle _viewRtv;
		GpuTexture* _viewDepthGpuTexture = nullptr;
		u32 _pipelineStateCount = 0;
		u32 _debugVisualizeType = 0;
		const u8* _enabledFlags = nullptr;
	};

	struct GeometryPassDesc {
		rhi::CommandList* _commandList = nullptr;
		rhi::RootSignature* _rootSignatures = nullptr;
		rhi::PipelineState* _pipelineStates = nullptr;
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::CpuDescriptorHandle _viewDsv;
		IndirectArgumentResource* _indirectArgumentResource = nullptr;
		VisibilityBufferFrameResource* _frameResource = nullptr;
		u32 _pipelineStateCount = 0;
		const u8* _enabledFlags = nullptr;
	};

	void initialize();
	void terminate();
	void buildShaderId(const BuildShaderIdDesc& desc);
	void shadingPass(const ShadingPassDesc& desc);
	void clearShaderId(const ClearShaderIdDesc& desc);
	void geometryPass(const GeometryPassDesc& desc);

	static VisiblityBufferRenderer* Get();

private:
	u32 _shadingQuadCount = 0;
	rhi::RootSignature _buildShaderIdRootSignature;
	rhi::PipelineState _buildShaderIdPipelineState;
	rhi::CommandSignature _commandSignature;

	GpuBuffer _buildShaderIdConstantBuffer;
	GpuBuffer _shadingConstantBuffer;

	DescriptorHandle _buildShaderIdCbv;
	DescriptorHandle _shadingCbv;
};
}