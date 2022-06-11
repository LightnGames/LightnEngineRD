#pragma once
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
namespace gpu {
struct MeshInstance {
	u32 _stateFlags = 0;
	u32 _meshIndex = 0;
	u32 _lodMeshInstanceOffset = 0;
	f32 _boundsRadius;
	Float3 _aabbMin;
	Float3 _aabbMax;
	f32 _worldScale = 0.0f;
	Float3x4 _worldMatrix;
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

	rhi::VertexBufferView getMeshInstanceIndexVertexBufferView() const;
	rhi::GpuDescriptorHandle getMeshInstanceGpuSrv() const { return _meshInstanceSrv._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getSubMeshInstanceOffsetsGpuSrv() const { return _subMeshInstanceOffsetsSrv._gpuHandle; }

	const u32* getSubMeshInstanceCounts() const { return _subMeshInstanceCounts; }
	const u32* getSubMeshInstanceOffsets() const { return _subMeshInstanceOffsets; }
	u32 getMeshInstanceReserveCount() const { return _meshInstanceReserveCount; };

	static GpuMeshInstanceManager* Get();

private:
	GpuBuffer _meshInstanceGpuBuffer;
	GpuBuffer _lodMeshInstanceGpuBuffer;
	GpuBuffer _subMeshInstanceGpuBuffer;
	GpuBuffer _subMeshInstanceOffsetsGpuBuffer;
	GpuBuffer _meshInstanceIndexGpuBuffer;
	DescriptorHandles _meshInstanceSrv;
	DescriptorHandle _subMeshInstanceOffsetsSrv;
	u32* _subMeshInstanceOffsets = nullptr;
	u32* _subMeshInstanceCounts = nullptr;
	u32 _meshInstanceReserveCount = 0;
};
}