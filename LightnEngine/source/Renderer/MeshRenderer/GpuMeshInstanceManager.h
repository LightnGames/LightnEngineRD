#pragma once
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
namespace gpu {
struct MeshInstanceDynamicData {
	f32 _boundsRadius = 0.0f;
	Float3 _aabbMin;
	Float3 _aabbMax;
	f32 _worldScale = 0.0f;
	Float3x4 _worldMatrix;
};

struct MeshInstance {
	u32 _stateFlags = 0;
	u32 _meshIndex = 0;
	u32 _lodMeshInstanceOffset = 0;
	MeshInstanceDynamicData _dynamicData;
};

struct LodMeshInstance {
	u32 _subMeshInstanceOffset = 0;
	f32 _lodThreshhold = 0.0f;
};

struct SubMeshInstance {
	u32 _materialParameterOffset = 0;
	u32 _materialIndex = 0;
};
}
class GpuMeshInstanceManager {
public:
	void initialize();
	void terminate();
	void update();

	rhi::VertexBufferView getSubMeshInstanceIndexVertexBufferView() const;
	rhi::GpuDescriptorHandle getMeshInstanceGpuSrv() const { return _meshInstanceSrv._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getSubMeshDrawOffsetsGpuSrv() const { return _subMeshDrawOffsetsSrv._gpuHandle; }

	const u32* getPipelineSetSubMeshInstanceCounts() const { return _pipelineSetSubMeshInstanceCounts; }
	const u32* getPipelineSetSubMeshInstanceOffsets() const { return _pipelineSetSubMeshInstanceOffsets; }
	u32 getMeshInstanceReserveCount() const { return _meshInstanceReserveCount; };

	static GpuMeshInstanceManager* Get();

private:
	GpuBuffer _meshInstanceGpuBuffer;
	GpuBuffer _lodMeshInstanceGpuBuffer;
	GpuBuffer _subMeshInstanceGpuBuffer;
	GpuBuffer _subMeshDrawOffsetsGpuBuffer;
	GpuBuffer _subMeshInstanceIndexGpuBuffer;
	DescriptorHandles _meshInstanceSrv;
	DescriptorHandle _subMeshDrawOffsetsSrv;
	u32* _subMeshDrawOffsets = nullptr;
	u32* _subMeshDrawCounts = nullptr;
	u32* _pipelineSetSubMeshInstanceOffsets = nullptr;
	u32* _pipelineSetSubMeshInstanceCounts = nullptr;
	u32 _meshInstanceReserveCount = 0;
};
}