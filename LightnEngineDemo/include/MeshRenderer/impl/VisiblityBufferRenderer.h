#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>

class VisiblityBufferRenderer {
public:
	static constexpr u32 MATERIAL_RANGE_TILE_SIZE = 64; // px

	void initialize();
	void terminate();

private:
	RootSignature* _buildMaterialIdRootSignature = nullptr;
	PipelineState* _buildMaterialIdPipelineState = nullptr;

	GpuTexture _triangleIdTexture;
	GpuTexture _materialIdTexture;
	GpuTexture _materialRangeTexture;

	DescriptorHandle _triangleIdRtv;
	DescriptorHandle _triangleIdSrv;
	DescriptorHandle _materialIdDsv;
	DescriptorHandle _materialRangeSrv;
};