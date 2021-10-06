#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>

struct ViewInfo;
class VramShaderSet;
class PipelineStateGroup;

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

	struct BuildShaderIdContext {
		CommandList* _commandList = nullptr;
	};

	struct ShadingContext {
		CommandList* _commandList = nullptr;
		ViewInfo* _viewInfo = nullptr;
		VramShaderSet* _vramShaderSets = nullptr;
		PipelineStateGroup** _pipelineStates = nullptr;
		const u32* _indirectArgmentCounts = nullptr;
		GpuDescriptorHandle _primitiveIndicesSrv;
		GpuDescriptorHandle _vertexPositionSrv;
		GpuDescriptorHandle _meshInstanceWorldMatrixSrv;
		GpuDescriptorHandle _meshInstanceSrv;
		GpuDescriptorHandle _meshesSrv;
		GpuDescriptorHandle _currentLodLevelSrv;
	};

	void initialize();
	void terminate();
	void buildShaderId(const BuildShaderIdContext& context);
	void shading(const ShadingContext& context);

	DescriptorHandle getTriangleIdRtvs() const { return _triangleIdRtv; }

private:
	u32 _shadingQuadCount = 0;
	RootSignature* _buildShaderIdRootSignature = nullptr;
	PipelineState* _buildShaderIdPipelineState = nullptr;

	GpuTexture _triangleIdTexture;
	GpuTexture _triangleShaderIdTexture;
	GpuTexture _shaderIdDepth;
	GpuBuffer _shaderRangeBuffer[2];// [min, max]
	GpuBuffer _buildShaderIdConstantBuffer;
	GpuBuffer _shadingConstantBuffer;

	DescriptorHandle _buildShaderIdCbv;
	DescriptorHandle _shadingCbv;
	DescriptorHandle _triangleIdRtv;
	DescriptorHandle _triangleIdSrv;
	DescriptorHandle _shaderIdSrv;
	DescriptorHandle _shaderIdDsv;
	DescriptorHandle _shaderRangeSrv;
	DescriptorHandle _shaderRangeUav;
	DescriptorHandle _shaderRangeCpuUav;
};