#pragma once
#include <Core/ChunkAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
struct DefaultRootParam {
	enum {
		VIEW_INFO,
		MESH_INSTANCE,
		MATERIAL_PARAMETER,
		INDIRECT_ARGUMENT_SUB_INFO,
		TEXTURE,
		MESH_INSTANCE_LOD_LEVEL,
		MATERIAL_SCREEN_PERSENTAGE,
		COUNT
	};
};

class GpuMaterialManager {
public:
	void initialize();
	void terminate();
	void update();

	rhi::RootSignature* getDefaultRootSignatures() { return _defaultRootSignatures; }
	rhi::PipelineState* getDefaultPipelineStates() { return _defaultPipelineStates; }
	rhi::GpuDescriptorHandle getParameterGpuSrv() { return _parameterSrv._gpuHandle; }

	static GpuMaterialManager* Get();

private:
	void updateMaterialParameters();

private:
	rhi::RootSignature* _defaultRootSignatures = nullptr;
	rhi::PipelineState* _defaultPipelineStates = nullptr;

	GpuBuffer _parameterGpuBuffer;
	DescriptorHandle _parameterSrv;
	ChunkAllocator _chunkAllocator;
};
}