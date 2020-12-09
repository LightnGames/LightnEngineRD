#include <GfxCore/impl/ViewSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <Core/Application.h>

ViewSystemImpl _viewSystem;

void ViewSystemImpl::initialize() {
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	Application* app = ApplicationSystem::Get()->getApplication();
	u32 screenWidth = app->getScreenWidth();
	u32 screenHeight = app->getScreenHeight();

	// view ports
	{
		_mainView._viewPort._width = static_cast<f32>(screenWidth);
		_mainView._viewPort._height = static_cast<f32>(screenHeight);
		_mainView._viewPort._maxDepth = 1.0f;
		_mainView._scissorRect._right = static_cast<l32>(screenWidth);
		_mainView._scissorRect._bottom = static_cast<l32>(screenHeight);
	}

	// cbv
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(ViewConstant));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		_mainView._viewInfoBuffer.initialize(desc);
		_mainView._viewInfoBuffer.setDebugName("Main View Info");

		_mainView._depthPrePassViewInfoBuffer.initialize(desc);
		_mainView._depthPrePassViewInfoBuffer.setDebugName("Depth Prepass View Info");

		DescriptorHeapAllocator* allocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_mainView._cbvHandle = allocater->allocateDescriptors(1);
		_mainView._depthPrePassCbvHandle = allocater->allocateDescriptors(1);
		device->createConstantBufferView(_mainView._viewInfoBuffer.getConstantBufferViewDesc(), _mainView._cbvHandle._cpuHandle);
		device->createConstantBufferView(_mainView._depthPrePassViewInfoBuffer.getConstantBufferViewDesc(), _mainView._depthPrePassCbvHandle._cpuHandle); 
	}

	// depth stencil texture
	{

		ClearValue depthOptimizedClearValue = {};
		depthOptimizedClearValue._format = FORMAT_D32_FLOAT;
		depthOptimizedClearValue._depthStencil._depth = 1.0f;
		depthOptimizedClearValue._depthStencil._stencil = 0;

		GpuTextureDesc desc = {};
		desc._device = device;
		desc._format = FORMAT_D32_FLOAT;
		desc._optimizedClearValue = &depthOptimizedClearValue;
		desc._width = screenWidth;
		desc._height = screenHeight;
		desc._flags = RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		desc._initialState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		_mainView._depthTexture.initialize(desc);
		_mainView._depthTexture.setDebugName("Main View Depth");

		ShaderResourceViewDesc srvDesc = {};
		srvDesc._format = FORMAT_R32_FLOAT;
		srvDesc._viewDimension = SRV_DIMENSION_TEXTURE2D;
		srvDesc._texture2D._mipLevels = 1;
		_mainView._depthSrv = allocator->allocateDescriptors(1);
		device->createShaderResourceView(_mainView._depthTexture.getResource(), &srvDesc, _mainView._depthSrv._cpuHandle);
	}

	// hdr texture
	{
		ClearValue depthOptimizedClearValue = {};
		depthOptimizedClearValue._format = BACK_BUFFER_FORMAT;

		GpuTextureDesc desc = {};
		desc._device = device;
		desc._format = BACK_BUFFER_FORMAT;
		desc._optimizedClearValue = &depthOptimizedClearValue;
		desc._width = screenWidth;
		desc._height = screenHeight;
		desc._flags = RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		desc._initialState = RESOURCE_STATE_RENDER_TARGET;
		_mainView._hdrTexture.initialize(desc);
		_mainView._hdrTexture.setDebugName("Main View HDR");
	}

	// descriptors
	{
		_mainView._depthDsv = GraphicsSystemImpl::Get()->getDsvGpuDescriptorAllocator()->allocateDescriptors(1);
		device->createDepthStencilView(_mainView._depthTexture.getResource(), _mainView._depthDsv._cpuHandle);

		_mainView._hdrRtv = GraphicsSystemImpl::Get()->getRtvGpuDescriptorAllocator()->allocateDescriptors(1);
		device->createRenderTargetView(_mainView._hdrTexture.getResource(), _mainView._hdrRtv._cpuHandle);
	}
}

void ViewSystemImpl::terminate() {
	_mainView._viewInfoBuffer.terminate();
	_mainView._depthPrePassViewInfoBuffer.terminate();
	_mainView._depthTexture.terminate();
	_mainView._hdrTexture.terminate();

	// srv cbv uav
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		allocator->discardDescriptor(_mainView._cbvHandle);
		allocator->discardDescriptor(_mainView._depthPrePassCbvHandle);
		allocator->discardDescriptor(_mainView._depthSrv); 
	}

	// dsv rtv
	{
		GraphicsSystemImpl::Get()->getDsvGpuDescriptorAllocator()->discardDescriptor(_mainView._depthDsv);
		GraphicsSystemImpl::Get()->getRtvGpuDescriptorAllocator()->discardDescriptor(_mainView._hdrRtv);
	}
}

void ViewSystemImpl::update() {
	struct CameraInfo {
		Vector3 position;
		Vector3 cameraAngle;
		f32 fov;
		f32 depthPrePassDistance = 0.0f;
	};
	auto debug = DebugWindow::StartWindow<CameraInfo>("CameraInfo");
	DebugGui::DragFloat("depth pre pass distance", &debug.depthPrePassDistance);
	DebugWindow::DragFloat3("position", &debug.position._x, 0.1f);
	DebugWindow::SliderAngle("rotation", &debug.cameraAngle._y);
	DebugWindow::SliderAngle("fov", &debug.fov, 0.1f);
	DebugWindow::End();

	f32 farClip = 300;
	f32 nearClip = 0.1f;
	f32 aspectRate = _mainView._viewPort._width / _mainView._viewPort._height;
	Matrix4 cameraRotate = Matrix4::rotate(debug.cameraAngle);
	Matrix4 viewMatrix = cameraRotate * Matrix4::translate(debug.position);
	Matrix4 projectionMatrix = Matrix4::perspectiveFovLH(debug.fov, aspectRate, nearClip, farClip);
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	ViewConstant* viewConstant = vramUpdater->enqueueUpdate<ViewConstant>(&_mainView._viewInfoBuffer, 0);
	viewConstant->_matrixView = viewMatrix.inverse().transpose();
	viewConstant->_matrixProj = projectionMatrix.transpose();
	viewConstant->_matrixViewProj = viewConstant->_matrixProj * viewConstant->_matrixView;
	viewConstant->_position = debug.position.getFloat3();
	viewConstant->_nearAndFarClip._x = nearClip;
	viewConstant->_nearAndFarClip._y = farClip;
	viewConstant->_halfFovTanX = tanf(debug.fov / 2.0f) * aspectRate;
	viewConstant->_halfFovTanY = tanf(debug.fov / 2.0f);
	viewConstant->_viewPortSizeX = static_cast<u32>(_mainView._viewPort._width);
	viewConstant->_viewPortSizeY = static_cast<u32>(_mainView._viewPort._height);
	viewConstant->_upDirection = Matrix4::transformNormal(Vector3::Up, cameraRotate).getFloat3();

	f32 fovHalfTan = tanf(debug.fov / 2.0f);
	Vector3 sideForward = Vector3::Forward * fovHalfTan * aspectRate;
	Vector3 forward = Vector3::Forward * fovHalfTan;
	Vector3 rightNormal = Matrix4::transformNormal(Vector3::Right + sideForward, cameraRotate).getNormalize();
	Vector3 leftNormal = Matrix4::transformNormal(-Vector3::Right + sideForward, cameraRotate).getNormalize();
	Vector3 buttomNormal = Matrix4::transformNormal(Vector3::Up + forward, cameraRotate).getNormalize();
	Vector3 topNormal = Matrix4::transformNormal(-Vector3::Up + forward, cameraRotate).getNormalize();
	Vector3 nearNormal = Matrix4::transformNormal(Vector3::Forward, cameraRotate).getNormalize();
	Vector3 farNormal = Matrix4::transformNormal(-Vector3::Forward, cameraRotate).getNormalize();
	viewConstant->_frustumPlanes[0] = Float4(rightNormal._x, rightNormal._y, rightNormal._z, Vector3::dot(rightNormal, debug.position));
	viewConstant->_frustumPlanes[1] = Float4(leftNormal._x, leftNormal._y, leftNormal._z, Vector3::dot(leftNormal, debug.position));
	viewConstant->_frustumPlanes[2] = Float4(buttomNormal._x, buttomNormal._y, buttomNormal._z, Vector3::dot(buttomNormal, debug.position));
	viewConstant->_frustumPlanes[3] = Float4(topNormal._x, topNormal._y, topNormal._z, Vector3::dot(topNormal, debug.position));
	viewConstant->_frustumPlanes[4] = Float4(nearNormal._x, nearNormal._y, nearNormal._z, Vector3::dot(nearNormal, debug.position) + nearClip);
	viewConstant->_frustumPlanes[5] = Float4(farNormal._x, farNormal._y, farNormal._z, Vector3::dot(farNormal, debug.position) - farClip);

	// デプスプリパス用ビュー定数バッファ更新
	f32 depthPrePassFarClip = debug.depthPrePassDistance;
	ViewConstant* depthPrePassViewConstant = vramUpdater->enqueueUpdate<ViewConstant>(&_mainView._depthPrePassViewInfoBuffer, 0);
	*depthPrePassViewConstant = *viewConstant;
	depthPrePassViewConstant->_frustumPlanes[5] = Float4(farNormal._x, farNormal._y, farNormal._z, Vector3::dot(farNormal, debug.position) - depthPrePassFarClip);
	depthPrePassViewConstant->_nearAndFarClip._y = depthPrePassFarClip;

	_mainView._nearClip = nearClip;
	_mainView._farClip = farClip;
	_mainView._viewMatrix = viewMatrix;
	_mainView._projectionMatrix = projectionMatrix;

	DebugWindow::StartWindow("Depth Texture");
	DebugGui::Columns(1, nullptr, true);
	ResourceDesc desc = _mainView._depthTexture.getResourceDesc();
	DebugGui::Text("[0] width:%-4d height:%-4d", desc._width, desc._height);
	DebugGui::Image(_mainView._depthSrv._gpuHandle, Vector2(200 * aspectRate, 200),
		Vector2(0, 0), Vector2(1, 1), Color4::WHITE, Color4::BLACK, DebugGui::COLOR_CHANNEL_FILTER_R, Vector2(0.95f, 1));
	DebugGui::NextColumn();
	DebugGui::Columns(1);
	DebugWindow::End();
}

void ViewSystemImpl::processDeletion() {
}

ViewSystemImpl* ViewSystemImpl::Get() {
	return &_viewSystem;
}
