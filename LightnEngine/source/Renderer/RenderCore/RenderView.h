#pragma once
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/GpuTexture.h>
#include <RendererScene/View.h>
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

    void setUpView(rhi::CommandList* commandList, const View& view,u32 viewIndex);

    GpuTexture* getViewColorTexture(u32 index) { return &_viewColorTextures[index]; }
	rhi::GpuDescriptorHandle getViewCbv(u32 index) const { return _viewCbv.get(index)._gpuHandle; }
    rhi::GpuDescriptorHandle getViewRtv(u32 index) const { return _viewRtv.get(index)._gpuHandle; }
    rhi::GpuDescriptorHandle getViewDsv(u32 index) const { return _viewDsv.get(index)._gpuHandle; }
    rhi::GpuDescriptorHandle getViewSrv(u32 index) const { return _viewSrv.get(index)._gpuHandle; }
    rhi::GpuDescriptorHandle getViewDepthSrv(u32 index) const { return _viewDepthSrv.get(index)._gpuHandle; }
    rhi::ViewPort createViewPort(const View& view) const { return { 0,0, f32(view.getWidth()), f32(view.getHeight()), 0, 1 }; }
    rhi::Rect createScissorRect(const View& view) const { return { 0,0, l32(view.getWidth()), l32(view.getHeight()) }; }

	static RenderViewScene* Get();

private:
    void updateGpuView(const View& view);

private:
    GpuTexture _viewColorTextures[ViewScene::VIEW_COUNT_MAX];
    GpuTexture _viewDepthTextures[ViewScene::VIEW_COUNT_MAX];
	GpuBuffer _viewConstantBuffer;
	DescriptorHandles _viewCbv;
    DescriptorHandles _viewRtv;
    DescriptorHandles _viewDsv;
    DescriptorHandles _viewSrv;
    DescriptorHandles _viewDepthSrv;
};
}