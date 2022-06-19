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
#include <Renderer/RenderCore/GpuTimeStampManager.h>

namespace ltn {
rhi::PipelineState _pipelineState;
rhi::RootSignature _rootSignature;
void Renderer::initialize() {
	// デバイス
	DeviceManager* deviceManager = DeviceManager::Get();
	deviceManager->initialize();

	// コマンドキュー
	rhi::Device* device = deviceManager->getDevice();
	rhi::CommandQueueDesc commandQueueDesc = {};
	commandQueueDesc._device = device;
	commandQueueDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
	_commandQueue.initialize(commandQueueDesc);

	// コマンドリストプール
	CommandListPool::Desc commandListPoolDesc = {};
	commandListPoolDesc._device = device;
	commandListPoolDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
	_commandListPool.initialize(commandListPoolDesc);

	// スワップチェーン
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

	// デスクリプタアロケーター
	DescriptorAllocatorGroup::Desc descriptorAllocatorDesc = {};
	descriptorAllocatorDesc._device = device;
	descriptorAllocatorDesc._rtvCount = 16;
	descriptorAllocatorDesc._dsvCount = 16;
	descriptorAllocatorDesc._srvCbvUavCpuCount = 128;
	descriptorAllocatorDesc._srvCbvUavGpuCount = 1024 * 2;
	DescriptorAllocatorGroup::Get()->initialize(descriptorAllocatorDesc);

	// グローバル ビデオメモリアロケーター
	rhi::VideoMemoryAllocatorDesc videoMemoryAllocatorDesc = {};
	videoMemoryAllocatorDesc._device = device;
	videoMemoryAllocatorDesc._hardwareAdapter = deviceManager->getHardwareAdapter();
	GlobalVideoMemoryAllocator::Get()->initialize(videoMemoryAllocatorDesc);

	// Vram アップデーター
	VramUpdater::Get()->initialize();

	// ImGui
	ImGuiSystem::Desc imguiSystemDesc = {};
	imguiSystemDesc._device = device;
	imguiSystemDesc._descriptorCount = 256;
	imguiSystemDesc._windowHandle = app->getWindowHandle();
	ImGuiSystem::Get()->initialize(imguiSystemDesc);

	// レンダーディレクター
	RenderDirector::Get()->initialize();

	// パイプラインステートリローダー
	PipelineStateReloader::Get()->initialize();

	// GPU タイムスタンプマネージャー
	GpuTimeStampManager::Get()->initialize();

	// スワップチェーンのバックバッファを取得
	for (u32 backBufferIndex = 0; backBufferIndex < rhi::BACK_BUFFER_COUNT; ++backBufferIndex) {
		GpuTexture& rtvTexture = _backBuffers[backBufferIndex];
		rtvTexture.initializeFromBackbuffer(&_swapChain, backBufferIndex);
		rtvTexture.setName("SwapChainBackBuffer[%d]", backBufferIndex);
	}

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

		vertexShader.terminate();
		pixelShader.terminate();
	}
}

void Renderer::terminate() {
	RenderDirector::Get()->terminate();

	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		_backBuffers[i].terminate();
	}

	_pipelineState.terminate();
	_rootSignature.terminate();

	PipelineStateReloader::Get()->terminate();
	ImGuiSystem::Get()->terminate();

	GpuTimeStampManager::Get()->terminate();
	DescriptorAllocatorGroup::Get()->terminate();
	_commandListPool.terminate();
	_commandQueue.terminate();
	_swapChain.terminate();

	VramUpdater::Get()->terminate();
	GlobalVideoMemoryAllocator::Get()->terminate();
	DeviceManager::Get()->terminate();
}

void Renderer::update() {
	GpuTimeStampManager::Get()->update(_frameIndex);
	VramUpdater::Get()->update(_frameIndex);
	PipelineStateReloader::Get()->update();
}

void Renderer::render() {
	moveToNextFrame();

	u64 completedFenceValue = _commandQueue.getCompletedValue();
	rhi::CommandList* commandList = _commandListPool.allocateCommandList(completedFenceValue);
	commandList->reset();

	// Vram アップデーター
	{
		VramUpdater::Get()->populateCommandList(commandList);
	}

	// メイン描画
	{
		RenderDirector::Get()->render(commandList);
		ImGuiSystem::Get()->render(commandList);
	}

	// View テクスチャからバックバッファにコピー
	{
		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4(), "CopyBackBuffer");
		RenderViewScene* renderViewScene = RenderViewScene::Get();
		GpuTexture* mainViewTexture = renderViewScene->getViewColorTexture(0);
		GpuTexture* backBuffer = &_backBuffers[_frameIndex];

		ScopedBarrierDesc barriers[] = {
			ScopedBarrierDesc(backBuffer, rhi::RESOURCE_STATE_COPY_DEST),
			ScopedBarrierDesc(mainViewTexture, rhi::RESOURCE_STATE_COPY_SOURCE)
		};
		ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));
		commandList->copyResource(backBuffer->getResource(), mainViewTexture->getResource());
	}

	_commandQueue.executeCommandLists(1, &commandList);
	_swapChain.present(1, 0);
}

void Renderer::waitForIdle() {
	_commandQueue.waitForIdle();
}

void Renderer::moveToNextFrame() {
	_commandQueue.waitForFence(_fenceValues[_frameIndex]);
	_frameIndex = _swapChain.getCurrentBackBufferIndex();

	ReleaseQueue::Get()->update();

	_fenceValues[_frameIndex] = _commandQueue.getNextFenceValue();
}

Renderer g_renderer;
Renderer* Renderer::Get() {
	return &g_renderer;
}
}
