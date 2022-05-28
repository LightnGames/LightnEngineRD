#pragma once
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
namespace gpu {
struct MeshInstance {
	u32 _meshIndex = 0;
	u32 _lodMeshInstanceOffset = 0;
	f32 _boundsRadius;
	Float3 _aabbMin;
	Float3 _aabbMax;
	f32 _worldScale = 0.0f;
};

struct LodMeshInstance {
	u32 _subMeshInstanceOffset = 0;
	f32 _lodThreshhold = 0.0f;
};

struct SubMeshInstance {
	u32 _materialInstanceIndex = 0;
	u32 _materialIndex = 0;
};
}
class GpuMeshInstanceManager {
public:
	void initialize();
	void terminate();
	void update();

	rhi::GpuDescriptorHandle getMeshInstanceGpuDescriptors() const { return _meshInstanceSrv._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getSubMeshInstanceOffsetsGpuDescriptor() const { return _subMeshInstanceOffsetsSrv._gpuHandle; }

	const u32* getSubMeshInstanceOffsets() const { return _subMeshInstanceOffsets; }

	static GpuMeshInstanceManager* Get();

private:
	GpuBuffer _meshInstanceGpuBuffer;
	GpuBuffer _lodMeshInstanceGpuBuffer;
	GpuBuffer _subMeshInstanceGpuBuffer;
	GpuBuffer _subMeshInstanceOffsetsGpuBuffer;
	DescriptorHandles _meshInstanceSrv;
	DescriptorHandle _subMeshInstanceOffsetsSrv;
	u32* _subMeshInstanceOffsets = nullptr;
};
}