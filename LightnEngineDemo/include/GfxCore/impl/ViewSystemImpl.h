#pragma once

#include <Core/System.h>
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/impl/DescriptorHeap.h>
#include <GfxCore/impl/GpuResourceImpl.h>

struct ViewInfo {
	static constexpr u32 FRUSTUM_PLANE_COUNT = 6;
	ViewPort _viewPort;
	Rect _scissorRect;
	DescriptorHandle _viewInfoCbv;
	DescriptorHandle _cullingViewInfoCbv;
	DescriptorHandle _depthPrePassViewInfoCbv;
	DescriptorHandle _depthSrv;
	DescriptorHandle _depthDsv;
	DescriptorHandle _hdrRtv;
	DescriptorHandle _hdrSrv;
	GpuBuffer _viewInfoBuffer;
	GpuBuffer _cullingViewInfoBuffer;
	GpuBuffer _depthPrePassViewInfoBuffer;
	GpuTexture _hdrTexture;
	GpuTexture _depthTexture;
	f32 _nearClip;
	f32 _farClip;
	Matrix4 _viewMatrix;
	Matrix4 _projectionMatrix;
	Matrix4 _cullingViewMatrix;
	Matrix4 _cullingProjectionMatrix;
};

struct ViewConstant {
	Matrix4 _matrixView;
	Matrix4 _matrixProj;
	Matrix4 _matrixViewProj;
	Float3 _position;
	u32 _padding1;
	Float2 _nearAndFarClip;
	f32 _halfFovTanX;
	f32 _halfFovTanY;
	u32 _viewPortSizeX;
	u32 _viewPortSizeY;
	u32 _padding2[2];
	Float3 _upDirection;
	u32 _padding4;
	Float4 _frustumPlanes[ViewInfo::FRUSTUM_PLANE_COUNT];
};

class LTN_GFX_CORE_API ViewSystemImpl {
public:
	void initialize();
	void terminate();
	void update();
	void processDeletion();

	ViewInfo* getView() { return &_mainView; }
	
	static ViewSystemImpl* Get();
private:
	ViewInfo _mainView;
};