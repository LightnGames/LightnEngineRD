#pragma once
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
namespace ltn {
namespace gpu {
constexpr u32 FRUSTUM_PLANE_COUNT = 6;
	struct View {
        Float4x4 _matrixView;
        Float4x4 _matrixProj;
        Float4x4 _matrixViewProj;
        Float3 _cameraPosition;
        u32 _padding1;
        Float2 _nearAndFarClip;
        Float2 _halfFovTan;
        u32 _viewPortSize[2];
        u32 _padding2[2];
        Float3 _upDirection;
        u32 _debugVisualizeType;
        Float4 _frustumPlanes[FRUSTUM_PLANE_COUNT];
	};
}

class RenderViewScene {
public:
	void initialize();
	void terminate();
	void update();

	rhi::GpuDescriptorHandle getViewConstantGpuDescriptor(u32 index) const { return _viewDescriptors.get(index)._gpuHandle; }

	static RenderViewScene* Get();

private:
	GpuBuffer _viewConstantBuffer;
	DescriptorHandles _viewDescriptors;
};
}