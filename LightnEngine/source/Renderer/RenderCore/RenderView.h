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

struct CullingResult {
	u32 _testFrustumCullingSubMeshInstanceCount;
	u32 _passFrustumCullingSubMeshInstanceCount;
	u32 _testOcclusionCullingSubMeshInstanceCount;
	u32 _passOcclusionCullingSubMeshInstanceCount;
	u32 _testOcclusionCullingMeshInstanceCount;
	u32 _passOcclusionCullingMeshInstanceCount;
	u32 _testFrustumCullingMeshInstanceCount;
	u32 _passFrustumCullingMeshInstanceCount;
	u32 _testFrustumCullingTriangleCount;
	u32 _passFrustumCullingTriangleCount;
	u32 _testOcclusionCullingTriangleCount;
	u32 _passOcclusionCullingTriangleCount;
};
}

struct RenderViewFrameResource {
	void setUpFrameResource(const View* view, rhi::CommandList* commandList);

	GpuBuffer* _cullingResultGpuBuffer = nullptr;
	GpuTexture* _viewColorTexture = nullptr;
	GpuTexture* _viewDepthTexture = nullptr;
	DescriptorHandle _viewRtv;
	DescriptorHandle _viewDsv;
	DescriptorHandle _viewSrv;
	DescriptorHandle _viewDepthSrv;
	DescriptorHandle _cullingResultUav;
	DescriptorHandle _cullingResultCpuUav;
};

class RenderViewScene {
public:
	void initialize();
	void terminate();
	void update();

	void setViewports(rhi::CommandList* commandList, const View& view, u32 viewIndex);
	void resetCullingResult(rhi::CommandList* commandList, RenderViewFrameResource* frameResource, u32 viewIndex);

	rhi::GpuDescriptorHandle getViewCbv(u32 index) const { return _viewCbv.get(index)._gpuHandle; }
	rhi::ViewPort createViewPort(const View& view) const { return { 0,0, f32(view.getWidth()), f32(view.getHeight()), 0, 1 }; }
	rhi::Rect createScissorRect(const View& view) const { return { 0,0, l32(view.getWidth()), l32(view.getHeight()) }; }

	void setMainViewGpuTexture(GpuTexture* texture) { _mainViewTexture = texture; }
	GpuTexture* getMainViewTexture() { return _mainViewTexture; }

	static RenderViewScene* Get();

private:
	void updateGpuView(const View& view, u32 viewIndex);
	void showCullingResultStatus(u32 viewIndex) const;

private:
	GpuTexture* _mainViewTexture = nullptr;
	GpuBuffer _cullingResultReadbackBuffer[ViewScene::VIEW_COUNT_MAX];
	GpuBuffer _viewConstantBuffers[ViewScene::VIEW_COUNT_MAX];
	DescriptorHandles _viewCbv;

	gpu::CullingResult _cullingResult[ViewScene::VIEW_COUNT_MAX];
};
}