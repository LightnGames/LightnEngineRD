#pragma once

#include <Core/System.h>
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/impl/DescriptorHeap.h>
#include <GfxCore/impl/GpuResourceImpl.h>

constexpr u32 FRUSTUM_PLANE_COUNT = 6;
struct ViewConstantInfo {
	Matrix4 _viewMatrix;
	Matrix4 _projectionMatrix;
	Vector4 _frustumPlanes[FRUSTUM_PLANE_COUNT];
	f32 _nearClip;
	f32 _farClip;
};

struct ViewInfo {
	enum DebugVisualizeType {
		DEBUG_VISUALIZE_TYPE_NONE = 0,
		DEBUG_VISUALIZE_TYPE_LOD,
		DEBUG_VISUALIZE_TYPE_MESHLET,
		DEBUG_VISUALIZE_TYPE_TEXCOORD,
		DEBUG_VISUALIZE_TYPE_DEPTH,
		DEBUG_VISUALIZE_TYPE_WIREFRAME,
		DEBUG_VISUALIZE_TYPE_BASECOLOR,
	};

	void initialize();
	void terminate();
	void debugDrawDepth();

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
	ViewConstantInfo _mainViewConstantInfo;
	ViewConstantInfo _cullingViewConstantInfo;
	DebugVisualizeType _debugVisualizeType = DEBUG_VISUALIZE_TYPE_NONE;
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
	u32 _debugVisualizeType;
	Float4 _frustumPlanes[FRUSTUM_PLANE_COUNT];
};

class LTN_GFX_CORE_API ViewSystemImpl {
public:
	void initialize();
	void terminate();
	void update();
	void processDeletion();

	ViewInfo* getView() { return &_mainView; }
	ViewInfo* getDebugFixedView() { return &_debugFixedView; }
	bool isEnabledDebugFixedView() const { return _isEnabledDebugFixedView; }
	
	static ViewSystemImpl* Get();
private:
	bool _isEnabledDebugFixedView = false;
	ViewInfo _mainView;
	ViewInfo _debugFixedView;
};