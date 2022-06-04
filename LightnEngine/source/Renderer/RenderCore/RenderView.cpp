#include "RenderView.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <RendererScene/View.h>

namespace ltn {
namespace {
RenderViewScene g_renderViewScene;
}
void RenderViewScene::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	u32 alignedViewSize = rhi::GetConstantBufferAligned(sizeof(gpu::View));

	// GPU リソース
	GpuBufferDesc desc = {};
	desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	desc._device = device;
	desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
	desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	desc._sizeInByte = ViewScene::VIEW_COUNT_MAX * alignedViewSize;
	_viewConstantBuffer.initialize(desc);

	// デスクリプタのみ初期化時に確保しておく
	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	_viewCbv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewSrv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewRtv = descriptorAllocatorGroup->getRtvAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewDsv = descriptorAllocatorGroup->getDsvAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
	_viewDepthSrv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(ViewScene::VIEW_COUNT_MAX);
}

void RenderViewScene::terminate() {
	_viewConstantBuffer.terminate();

	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewCbv);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewSrv);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_viewDepthSrv);
	descriptorAllocatorGroup->getRtvAllocator()->free(_viewRtv);
	descriptorAllocatorGroup->getDsvAllocator()->free(_viewDsv);

	ViewScene* viewScene = ViewScene::Get();
	for (u32 i = 0; i < ViewScene::VIEW_COUNT_MAX; ++i) {
		if (viewScene->getViewEnabledFlags()[i] == 0) {
			continue;
		}
		_viewColorTextures[i].terminate();
		_viewDepthTextures[i].terminate();
	}
}

void RenderViewScene::update() {
	ViewScene* viewScene = ViewScene::Get();
	const View* views = viewScene->getView(0);
	const u8* stateFlags = viewScene->getViewStateFlags();
	for (u32 i = 0; i < ViewScene::VIEW_COUNT_MAX; ++i) {
		if (stateFlags[i] == ViewScene::VIEW_STATE_NONE) {
			continue;
		}

		if (stateFlags[i] & ViewScene::VIEW_STATE_CREATED) {
			rhi::Device* device = DeviceManager::Get()->getDevice();
			const View& view = views[i];

			// カラーテクスチャ
			{
				rhi::ClearValue optimizedClearValue = {};
				optimizedClearValue._format = rhi::BACK_BUFFER_FORMAT;

				GpuTextureDesc desc = {};
				desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
				desc._device = device;
				desc._format = rhi::BACK_BUFFER_FORMAT;
				desc._optimizedClearValue = &optimizedClearValue;
				desc._width = view.getWidth();
				desc._height = view.getHeight();
				desc._flags = rhi::RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				desc._initialState = rhi::RESOURCE_STATE_RENDER_TARGET;
				_viewColorTextures[i].initialize(desc);
				_viewColorTextures[i].setName("ViewColor[%d]", i);
			}

			// デプステクスチャ
			{
				rhi::ClearValue depthOptimizedClearValue = {};
				depthOptimizedClearValue._format = rhi::FORMAT_D32_FLOAT;
				depthOptimizedClearValue._depthStencil._depth = 1.0f;
				depthOptimizedClearValue._depthStencil._stencil = 0;

				GpuTextureDesc desc = {};
				desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
				desc._device = device;
				desc._format = rhi::FORMAT_D32_FLOAT;
				desc._optimizedClearValue = &depthOptimizedClearValue;
				desc._width = view.getWidth();
				desc._height = view.getHeight();
				desc._flags = rhi::RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				desc._initialState = rhi::RESOURCE_STATE_DEPTH_WRITE;
				_viewDepthTextures[i].initialize(desc);
				_viewDepthTextures[i].setName("ViewDepth[%d]", i);
			}

			// 定数バッファビュー
			{
				u32 alignedViewSize = rhi::GetConstantBufferAligned(sizeof(gpu::View));
				rhi::ConstantBufferViewDesc desc;
				desc._bufferLocation = _viewConstantBuffer.getGpuVirtualAddress() + alignedViewSize * i;
				desc._sizeInBytes = alignedViewSize;
				device->createConstantBufferView(desc, _viewCbv._firstHandle._cpuHandle + _viewCbv._incrementSize * i);
			}

			// デスクリプター
			{
				device->createDepthStencilView(_viewDepthTextures[i].getResource(), _viewDsv.get(i)._cpuHandle);
				device->createRenderTargetView(_viewColorTextures[i].getResource(), _viewRtv.get(i)._cpuHandle);
				device->createShaderResourceView(_viewColorTextures[i].getResource(), nullptr, _viewSrv.get(i)._cpuHandle);

				rhi::ShaderResourceViewDesc srvDesc = {};
				srvDesc._format = rhi::FORMAT_R32_FLOAT;
				srvDesc._viewDimension = rhi::SRV_DIMENSION_TEXTURE2D;
				srvDesc._texture2D._mipLevels = 1;
				device->createShaderResourceView(_viewDepthTextures[i].getResource(), &srvDesc, _viewDepthSrv.get(i)._cpuHandle);
			}
		}

		if (stateFlags[i] & ViewScene::VIEW_STATE_UPDATED) {
			const View& view = views[i];
			const Camera* camera = view.getCamera();
			f32 farClip = camera->_farClip;
			f32 nearClip = camera->_nearClip;
			f32 fov = camera->_fov;
			f32 fovHalfTan = tanf(fov / 2.0f);
			f32 aspectRate = view.getAspectRate();
			Vector3 cameraPosition = camera->_worldMatrix.getCol(3).getVector3();
			Matrix4 viewMatrix = camera->_worldMatrix.inverse();
			Matrix4 projectionMatrix = Matrix4::perspectiveFovLH(fov, aspectRate, nearClip, farClip);
			Matrix4 viewProjectionMatrix = projectionMatrix * viewMatrix;

			gpu::View* gpuView = VramUpdater::Get()->enqueueUpdate<gpu::View>(&_viewConstantBuffer);
			gpuView->_matrixView = viewMatrix.transpose().getFloat4x4();
			gpuView->_matrixProj = projectionMatrix.transpose().getFloat4x4();
			gpuView->_matrixViewProj = viewProjectionMatrix.transpose().getFloat4x4();
			gpuView->_cameraPosition = cameraPosition.getFloat3();
			gpuView->_nearAndFarClip.x = nearClip;
			gpuView->_nearAndFarClip.y = farClip;
			gpuView->_halfFovTan.x = fovHalfTan * aspectRate;
			gpuView->_halfFovTan.y = fovHalfTan;
			gpuView->_viewPortSize[0] = view.getWidth();
			gpuView->_viewPortSize[1] = view.getHeight();
			gpuView->_upDirection = camera->_worldMatrix.getCol(1).getFloat3();
		}

		if (stateFlags[i] & ViewScene::VIEW_STATE_DESTROY) {
			_viewColorTextures[i].terminate();
			_viewDepthTextures[i].terminate();
		}
	}
}

void RenderViewScene::setUpView(rhi::CommandList* commandList, const View& view,u32 viewIndex) {
	f32 clearColor[4] = {};
	rhi::CpuDescriptorHandle rtv = _viewRtv.get(viewIndex)._cpuHandle;
	rhi::CpuDescriptorHandle dsv = _viewDsv.get(viewIndex)._cpuHandle;
	rhi::ViewPort viewPort = createViewPort(view);
	rhi::Rect scissorRect = createScissorRect(view);
	commandList->setRenderTargets(1, &rtv, &dsv);
	commandList->clearRenderTargetView(rtv, clearColor);
	commandList->setViewports(1, &viewPort);
	commandList->setScissorRects(1, &scissorRect);
}

RenderViewScene* RenderViewScene::Get() {
	return &g_renderViewScene;
}
}
