#include "Renderer.h"
#include <Core/Utility.h>
#include <Application/Application.h>
#include <Renderer/RenderCore/RendererUtility.h>

namespace ltn {
rhi::PipelineState _pipelineState;
rhi::RootSignature _rootSignature;
void Renderer::initialize() {
	rhi::HardwareFactoryDesc factoryDesc = {};
	factoryDesc._flags = rhi::HardwareFactoryDesc::FACTROY_FLGA_DEVICE_DEBUG;
	_factory.initialize(factoryDesc);

	rhi::HardwareAdapterDesc adapterDesc = {};
	adapterDesc._factory = &_factory;
	_adapter.initialize(adapterDesc);

	rhi::DeviceDesc deviceDesc = {};
	deviceDesc._adapter = &_adapter;
	_device.initialize(deviceDesc);

	rhi::CommandQueueDesc commandQueueDesc = {};
	commandQueueDesc._device = &_device;
	commandQueueDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
	_commandQueue.initialize(commandQueueDesc);

	CommandListPool::Desc commandListPoolDesc = {};
	commandListPoolDesc._device = &_device;
	commandListPoolDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
	_commandListPool.initialize(commandListPoolDesc);

	Application* app = ApplicationSysytem::Get()->getApplication();
	rhi::SwapChainDesc swapChainDesc = {};
	swapChainDesc._bufferingCount = rhi::BACK_BUFFER_COUNT;
	swapChainDesc._format = rhi::BACK_BUFFER_FORMAT;
	swapChainDesc._width = app->getScreenWidth();
	swapChainDesc._height = app->getScreenHeight();
	swapChainDesc._commandQueue = &_commandQueue;
	swapChainDesc._hWnd = app->getWindowHandle();
	swapChainDesc._factory = &_factory;
	_swapChain.initialize(swapChainDesc);

	// デスクリプタアロケーター初期化
	DescriptorAllocatorGroup::Desc descriptorAllocatorDesc = {};
	descriptorAllocatorDesc._device = &_device;
	descriptorAllocatorDesc._rtvCount = 16;
	descriptorAllocatorDesc._dsvCount = 16;
	descriptorAllocatorDesc._srvCbvUavCpuCount = 128;
	descriptorAllocatorDesc._srvCbvUavGpuCount = 1024;
	_descriptorAllocatorGroup.initialize(descriptorAllocatorDesc);

	// スワップチェーンのバックバッファを取得
	for (u32 backBufferIndex = 0; backBufferIndex < rhi::BACK_BUFFER_COUNT; ++backBufferIndex) {
		GpuTexture& rtvTexture = _backBuffers[backBufferIndex];
		rtvTexture.initializeFromBackbuffer(&_swapChain, backBufferIndex);
	}

	_rtvDescriptors = _descriptorAllocatorGroup.getRtvAllocator()->allocate(rhi::BACK_BUFFER_COUNT);
	for (u32 backBufferIndex = 0; backBufferIndex < rhi::BACK_BUFFER_COUNT; ++backBufferIndex) {
		_device.createRenderTargetView(_backBuffers[backBufferIndex].getResource(), _rtvDescriptors.get(backBufferIndex)._cpuHandle);
	}

	ImGuiSystem::Desc imguiSystemDesc = {};
	imguiSystemDesc._device = &_device;
	imguiSystemDesc._descriptorCount = 256;
	imguiSystemDesc._windowHandle = app->getWindowHandle();
	_imguiSystem.initialize(imguiSystemDesc);

	_pipelineStateReloader.initialize();

	{
		AssetPath vertexShaderPath("EngineComponent/Shader/ScreenTriangle.vso");
		AssetPath pixelShaderPath("EngineComponent/Shader/ScreenTriangle.pso");
		rhi::ShaderBlob vertexShader;
		rhi::ShaderBlob pixelShader;
		vertexShader.initialize(vertexShaderPath.get());
		pixelShader.initialize(pixelShaderPath.get());

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = &_device;
		_rootSignature.iniaitlize(rootSignatureDesc);

		rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = &_device;
		pipelineStateDesc._vs = vertexShader.getShaderByteCode();
		pipelineStateDesc._ps = pixelShader.getShaderByteCode();
		pipelineStateDesc._numRenderTarget = 1;
		pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc._rootSignature = &_rootSignature;
		pipelineStateDesc._sampleDesc._count = 1;
		_pipelineState.iniaitlize(pipelineStateDesc);

		vertexShader.terminate();
		pixelShader.terminate();
	}

	//QueryHeapSystem::Get()->initialize();
	//ViewSystemImpl::Get()->initialize();

	//_vramBufferUpdater.initialize();
	//_debugWindow.initialize();

	//// タイムスタンプのためにGPU周波数を取得
	//QueryHeapSystem::Get()->setGpuFrequency(_commandQueue);
	//QueryHeapSystem::Get()->setCpuFrequency();
}

void Renderer::terminate() {
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		_backBuffers[i].terminate();
	}
	_descriptorAllocatorGroup.getRtvAllocator()->free(_rtvDescriptors);

	_pipelineStateReloader.terminate();
	_imguiSystem.terminate();

	_pipelineState.terminate();
	_rootSignature.terminate();
	_descriptorAllocatorGroup.terminate();
	_commandListPool.terminate();
	_commandQueue.terminate();
	_swapChain.terminate();
	_adapter.terminate();
	_factory.terminate();
	_device.terminate();
}

void Renderer::update() {
	_pipelineStateReloader.update();
	_imguiSystem.beginFrame();
}

void Renderer::render() {
	u64 completedFenceValue = _commandQueue.getCompletedValue();
	rhi::CommandList* commandList = _commandListPool.allocateCommandList(completedFenceValue);

	f32 clearColor[4] = {};
	rhi::CpuDescriptorHandle rtvDescriptor = _rtvDescriptors.get(_frameIndex)._cpuHandle;

	commandList->reset();

	rhi::DescriptorHeap* descriptorHeaps[] = { _descriptorAllocatorGroup.getSrvCbvUavGpuAllocator()->getDescriptorHeap() };
	commandList->setDescriptorHeaps(LTN_COUNTOF(descriptorHeaps), descriptorHeaps);

	{
		ScopedBarrierDesc barriers[1] = {
			ScopedBarrierDesc(&_backBuffers[_frameIndex], rhi::RESOURCE_STATE_RENDER_TARGET)
		};
		ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

		rhi::ViewPort viewPort = { 0,0,1920,1080,0,1 };
		rhi::Rect scissorRect = { 0,0,1920,1080 };
		commandList->setRenderTargets(1, &rtvDescriptor, nullptr);
		commandList->clearRenderTargetView(rtvDescriptor, clearColor);
		commandList->setGraphicsRootSignature(&_rootSignature);
		commandList->setPipelineState(&_pipelineState);
		commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->setViewports(1, &viewPort);
		commandList->setScissorRects(1, &scissorRect);
		commandList->drawInstanced(3, 1, 0, 0);

		_imguiSystem.render(commandList);
	}

	_commandQueue.executeCommandLists(1, &commandList);
	_swapChain.present(1, 0);

	moveToNextFrame();
}

void Renderer::moveToNextFrame() {
	u64 currentFenceValue = _fenceValues[_frameIndex];
	_commandQueue.waitForFence(currentFenceValue);
	_frameIndex = _swapChain.getCurrentBackBufferIndex();

	u64 fenceValue = _commandQueue.incrimentFence();
	_fenceValues[_frameIndex] = fenceValue;
}

Renderer g_renderer;
Renderer* Renderer::Get() {
	return &g_renderer;
}
}
