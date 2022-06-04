#include "Renderer.h"
#include <Core/Utility.h>
#include <Application/Application.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/RenderCore/CommandListPool.h>
#include <Renderer/RenderCore/ImGuiSystem.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/RenderDirector.h>
#include <Renderer/RenderCore/RenderView.h>

namespace ltn {
rhi::PipelineState _pipelineState;
rhi::RootSignature _rootSignature;
void Renderer::initialize() {
	DeviceManager* deviceManager = DeviceManager::Get();
	deviceManager->initialize();

	rhi::Device* device = deviceManager->getDevice();
	rhi::CommandQueueDesc commandQueueDesc = {};
	commandQueueDesc._device = device;
	commandQueueDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
	_commandQueue.initialize(commandQueueDesc);

	CommandListPool::Desc commandListPoolDesc = {};
	commandListPoolDesc._device = device;
	commandListPoolDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
	CommandListPool::Get()->initialize(commandListPoolDesc);

	Application* app = ApplicationSysytem::Get()->getApplication();
	rhi::SwapChainDesc swapChainDesc = {};
	swapChainDesc._bufferingCount = rhi::BACK_BUFFER_COUNT;
	swapChainDesc._format = rhi::BACK_BUFFER_FORMAT;
	swapChainDesc._width = app->getScreenWidth();
	swapChainDesc._height = app->getScreenHeight();
	swapChainDesc._commandQueue = &_commandQueue;
	swapChainDesc._hWnd = app->getWindowHandle();
	swapChainDesc._factory = deviceManager->getHardwareFactory();
	_swapChain.initialize(swapChainDesc);

	// デスクリプタアロケーター初期化
	DescriptorAllocatorGroup::Desc descriptorAllocatorDesc = {};
	descriptorAllocatorDesc._device = device;
	descriptorAllocatorDesc._rtvCount = 16;
	descriptorAllocatorDesc._dsvCount = 16;
	descriptorAllocatorDesc._srvCbvUavCpuCount = 128;
	descriptorAllocatorDesc._srvCbvUavGpuCount = 1024;
	DescriptorAllocatorGroup::Get()->initialize(descriptorAllocatorDesc);

	// スワップチェーンのバックバッファを取得
	for (u32 backBufferIndex = 0; backBufferIndex < rhi::BACK_BUFFER_COUNT; ++backBufferIndex) {
		GpuTexture& rtvTexture = _backBuffers[backBufferIndex];
		rtvTexture.initializeFromBackbuffer(&_swapChain, backBufferIndex);
		rtvTexture.setName("SwapChainBackBuffer[%d]", backBufferIndex);
	}

	_rtvDescriptors = DescriptorAllocatorGroup::Get()->getRtvAllocator()->allocate(rhi::BACK_BUFFER_COUNT);
	for (u32 backBufferIndex = 0; backBufferIndex < rhi::BACK_BUFFER_COUNT; ++backBufferIndex) {
		device->createRenderTargetView(_backBuffers[backBufferIndex].getResource(), _rtvDescriptors.get(backBufferIndex)._cpuHandle);
	}

	// グローバル ビデオメモリアロケーター
	rhi::VideoMemoryAllocatorDesc videoMemoryAllocatorDesc = {};
	videoMemoryAllocatorDesc._device = device;
	videoMemoryAllocatorDesc._hardwareAdapter = deviceManager->getHardwareAdapter();
	GlobalVideoMemoryAllocator::Get()->initialize(videoMemoryAllocatorDesc);

	VramUpdater::Get()->initialize();

	// ImGui
	ImGuiSystem::Desc imguiSystemDesc = {};
	imguiSystemDesc._device = device;
	imguiSystemDesc._descriptorCount = 256;
	imguiSystemDesc._windowHandle = app->getWindowHandle();
	ImGuiSystem::Get()->initialize(imguiSystemDesc);

	PipelineStateReloader::Get()->initialize();

	{
		AssetPath vertexShaderPath("EngineComponent\\Shader\\ScreenTriangle.vso");
		AssetPath pixelShaderPath("EngineComponent\\Shader\\ScreenTriangle.pso");
		rhi::ShaderBlob vertexShader;
		rhi::ShaderBlob pixelShader;
		vertexShader.initialize(vertexShaderPath.get());
		pixelShader.initialize(pixelShaderPath.get());

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		_rootSignature.iniaitlize(rootSignatureDesc);
		_rootSignature.setName("RootSigScreenTriangle");

		rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._vs = vertexShader.getShaderByteCode();
		pipelineStateDesc._ps = pixelShader.getShaderByteCode();
		pipelineStateDesc._numRenderTarget = 1;
		pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc._rootSignature = &_rootSignature;
		pipelineStateDesc._sampleDesc._count = 1;
		_pipelineState.iniaitlize(pipelineStateDesc);
		_pipelineState.setName("PsoScreenTriangle");

		PipelineStateReloader::GraphicsPipelineStateRegisterDesc reloaderDesc = {};
		reloaderDesc._desc = pipelineStateDesc;
		reloaderDesc._shaderPaths[0] = vertexShaderPath.get();
		reloaderDesc._shaderPaths[1] = pixelShaderPath.get();
		PipelineStateReloader::Get()->registerPipelineState(&_pipelineState,reloaderDesc);

		vertexShader.terminate();
		pixelShader.terminate();
	}

	RenderDirector::Get()->initialize();

	//QueryHeapSystem::Get()->initialize();
	//ViewSystemImpl::Get()->initialize();

	//_vramBufferUpdater.initialize();
	//_debugWindow.initialize();

	//// タイムスタンプのためにGPU周波数を取得
	//QueryHeapSystem::Get()->setGpuFrequency(_commandQueue);
	//QueryHeapSystem::Get()->setCpuFrequency();
}

void Renderer::terminate() {
	RenderDirector::Get()->terminate();

	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		_backBuffers[i].terminate();
	}
	DescriptorAllocatorGroup::Get()->getRtvAllocator()->free(_rtvDescriptors);

	PipelineStateReloader::Get()->unregisterPipelineState(&_pipelineState);
	_pipelineState.terminate();
	_rootSignature.terminate();

	PipelineStateReloader::Get()->terminate();
	ImGuiSystem::Get()->terminate();

	DescriptorAllocatorGroup::Get()->terminate();
	CommandListPool::Get()->terminate();
	_commandQueue.terminate();
	_swapChain.terminate();

	VramUpdater::Get()->terminate();
	GlobalVideoMemoryAllocator::Get()->terminate();
	DeviceManager::Get()->terminate();
}

void Renderer::update() {
	VramUpdater::Get()->update(_frameIndex);
	PipelineStateReloader::Get()->update();
	ImGuiSystem::Get()->beginFrame();
}

void Renderer::render() {
	u64 completedFenceValue = _commandQueue.getCompletedValue();
	rhi::CommandList* commandList = CommandListPool::Get()->allocateCommandList(completedFenceValue);

	rhi::CpuDescriptorHandle rtvDescriptor = _rtvDescriptors.get(_frameIndex)._cpuHandle;

	commandList->reset();

	VramUpdater::Get()->populateCommandList(commandList);

	rhi::DescriptorHeap* descriptorHeaps[] = { DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->getDescriptorHeap() };
	commandList->setDescriptorHeaps(LTN_COUNTOF(descriptorHeaps), descriptorHeaps);

	{

		RenderDirector::Get()->render(commandList);
		ImGuiSystem::Get()->render(commandList);

		RenderViewScene* renderViewScene = RenderViewScene::Get();
		GpuTexture* mainViewTexture = renderViewScene->getViewColorTexture(0);
		GpuTexture* backBuffer = &_backBuffers[_frameIndex];

		ScopedBarrierDesc barriers[2] = {
			ScopedBarrierDesc(backBuffer, rhi::RESOURCE_STATE_COPY_DEST),
			ScopedBarrierDesc(mainViewTexture, rhi::RESOURCE_STATE_COPY_SOURCE)
		};
		ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));
		commandList->copyResource(backBuffer->getResource(), mainViewTexture->getResource());

		//f32 clearColor[4] = {};
		//rhi::ViewPort viewPort = { 0,0,1920,1080,0,1 };
		//rhi::Rect scissorRect = { 0,0,1920,1080 };
		//commandList->setRenderTargets(1, &rtvDescriptor, nullptr);
		//commandList->clearRenderTargetView(rtvDescriptor, clearColor);
		//commandList->setGraphicsRootSignature(&_rootSignature);
		//commandList->setPipelineState(&_pipelineState);
		//commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//commandList->setViewports(1, &viewPort);
		//commandList->setScissorRects(1, &scissorRect);
		//commandList->drawInstanced(3, 1, 0, 0);
	}

	_commandQueue.executeCommandLists(1, &commandList);
	_swapChain.present(1, 0);

	moveToNextFrame();
}

void Renderer::waitForIdle() {
	_commandQueue.waitForIdle();
}

void Renderer::moveToNextFrame() {
	_commandQueue.waitForFence(_fenceValues[_frameIndex]);
	_frameIndex = _swapChain.getCurrentBackBufferIndex();

	ReleaseQueue::Get()->update();

	u64 fenceValue = _commandQueue.incrimentFence();
	_fenceValues[_frameIndex] = fenceValue;
}

Renderer g_renderer;
Renderer* Renderer::Get() {
	return &g_renderer;
}
}
