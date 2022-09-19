#pragma once
#include <Core/ChunkAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
class Shader;
struct ShadingRootParam {
	enum {
		SHADING_INFO,
		PIPELINE_SET_INFO,
		PIPELINE_SET_RANGE,
		VIEW_INFO,
		MATERIAL_PARAMETER,
		MATERIAL_PARAMETER_OFFSET,
		MESH,
		MESH_INSTANCE,
		MESH_INSTANCE_LOD_LEVEL,
		MESH_INSTANCE_SCREEN_PERSENTAGE,
		MESH_LOD_STREAMED_LEVEL,
		MATERIAL_SCREEN_PERSENTAGE,
		GEOMETRY_GLOBAL_OFFSET,
		VERTEX_RESOURCE,
		TRIANGLE_ATTRIBUTE,
		VIEW_DEPTH,
		TEXTURE,
		DEBUG_TYPE,
		COUNT
	};
};

struct GeometryRootParam {
	enum {
		VIEW_INFO,
		MESH_INSTANCE,
		INDIRECT_ARGUMENT_SUB_INFO,
		COUNT
	};
};

class GpuMaterialManager {
public:
	void initialize();
	void terminate();
	void update();

	rhi::RootSignature* getGeometryPassRootSignatures() { return _geometryPassRootSignatures; }
	rhi::RootSignature* getShadingPassRootSignatures() { return _shadingPassRootSignatures; }
	rhi::PipelineState* getGeometryPassPipelineStates() { return _geometryPassPipelineStates; }
	rhi::PipelineState* getShadingPassPipelineStates() { return _shadingPassPipelineStates; }
	rhi::PipelineState* getDebugShadingPassPipelineStates() { return _debugVisualizePipelineStates; }
	rhi::GpuDescriptorHandle getParameterGpuSrv() { return _parameterSrv._gpuHandle; }
	rhi::GpuDescriptorHandle getParameterOffsetGpuSrv() { return _parameterOffsetSrv._gpuHandle; }

	static GpuMaterialManager* Get();

private:
	void updateMaterialParameters();
	void updateMaterialParameterOffsets();

private:
	rhi::RootSignature* _geometryPassRootSignatures = nullptr;
	rhi::PipelineState* _geometryPassPipelineStates = nullptr;
	rhi::RootSignature* _shadingPassRootSignatures = nullptr;
	rhi::PipelineState* _shadingPassPipelineStates = nullptr;
	rhi::PipelineState* _debugVisualizePipelineStates = nullptr;

	const Shader* _debugVisualizeShader = nullptr;
	GpuBuffer _parameterGpuBuffer;
	GpuBuffer _parameterOffsetGpuBuffer;
	DescriptorHandle _parameterSrv;
	DescriptorHandle _parameterOffsetSrv;
	ChunkAllocator _chunkAllocator;
};
}