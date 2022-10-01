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
#include <Renderer/RenderCore/GpuTimerManager.h>
#include <Renderer/RenderCore/FrameResourceAllocator.h>
#include <Renderer/RenderCore/DebugRenderer.h>

namespace ltn {
rhi::PipelineState _pipelineState;
rhi::RootSignature _rootSignature;
void Renderer::initialize() {
	CpuScopedPerf scopedPerf("Renderer");

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
	descriptorAllocatorDesc._rtvCount = 1024;
	descriptorAllocatorDesc._dsvCount = 1024;
	descriptorAllocatorDesc._srvCbvUavCpuCount = 1024;
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
	GpuTimerManager::Get()->initialize();

	// フレームアロケーター
	FrameBufferAllocator::Get()->initialize();
	FrameDescriptorAllocator::Get()->initialize();

	// デバッグレンダラー
	DebugRenderer::Get()->initialize();

	// スワップチェーンのバックバッファを取得
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		GpuTexture& rtvTexture = _backBuffers[i];
		rtvTexture.initializeFromBackbuffer(&_swapChain, i);
		rtvTexture.setName("SwapChainBackBuffer[%d]", i);
	}

	{
		rhi::DescriptorRange inputTextureSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		rhi::RootParameter rootParameters[CopyTextureRootParam::COUNT] = {};
		rootParameters[CopyTextureRootParam::INPUT_SRV].initializeDescriptorTable(1, &inputTextureSrvRange, rhi::SHADER_VISIBILITY_ALL);

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_copyToBackBufferRootSignature.iniaitlize(rootSignatureDesc);

		AssetPath vertexShaderPath("EngineComponent\\Shader\\ScreenTriangle.vso");
		AssetPath pixelShaderPath("EngineComponent\\Shader\\Utility\\CopyTexture.pso");
		rhi::ShaderBlob vertexShader;
		rhi::ShaderBlob pixelShader;
		vertexShader.initialize(vertexShaderPath.get());
		pixelShader.initialize(pixelShaderPath.get());

		rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._vs = vertexShader.getShaderByteCode();
		pipelineStateDesc._ps = pixelShader.getShaderByteCode();
		pipelineStateDesc._numRenderTarget = 1;
		pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc._rootSignature = &_copyToBackBufferRootSignature;
		pipelineStateDesc._sampleDesc._count = 1;
		_copyToBackBufferPipelineState.iniaitlize(pipelineStateDesc);
		_copyToBackBufferPipelineState.setName("CopyToTexturePipelineState");

		vertexShader.terminate();
		pixelShader.terminate();
	}

	_backBufferRtv = DescriptorAllocatorGroup::Get()->allocateRtvGpu(rhi::BACK_BUFFER_COUNT);
	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		device->createRenderTargetView(_backBuffers[i].getResource(), _backBufferRtv.get(i)._cpuHandle);
	}

	//{
	//	AssetPath vertexShaderPath("EngineComponent\\Shader\\ScreenTriangle.vso");
	//	AssetPath pixelShaderPath("EngineComponent\\Shader\\ScreenTriangle.pso");
	//	rhi::ShaderBlob vertexShader;
	//	rhi::ShaderBlob pixelShader;
	//	vertexShader.initialize(vertexShaderPath.get());
	//	pixelShader.initialize(pixelShaderPath.get());

	//	rhi::RootSignatureDesc rootSignatureDesc = {};
	//	rootSignatureDesc._device = device;
	//	_rootSignature.iniaitlize(rootSignatureDesc);
	//	_rootSignature.setName("RootSigScreenTriangle");

	//	rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
	//	pipelineStateDesc._device = device;
	//	pipelineStateDesc._vs = vertexShader.getShaderByteCode();
	//	pipelineStateDesc._ps = pixelShader.getShaderByteCode();
	//	pipelineStateDesc._numRenderTarget = 1;
	//	pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
	//	pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//	pipelineStateDesc._rootSignature = &_rootSignature;
	//	pipelineStateDesc._sampleDesc._count = 1;
	//	_pipelineState.iniaitlize(pipelineStateDesc);
	//	_pipelineState.setName("PsoScreenTriangle");

	//	vertexShader.terminate();
	//	pixelShader.terminate();
	//}
}

void Renderer::terminate() {
	RenderDirector::Get()->terminate();

	DebugRenderer::Get()->terminate();
	FrameBufferAllocator::Get()->terminate();
	FrameDescriptorAllocator::Get()->terminate();

	for (u32 i = 0; i < rhi::BACK_BUFFER_COUNT; ++i) {
		_backBuffers[i].terminate();
	}
	DescriptorAllocatorGroup::Get()->freeRtvGpu(_backBufferRtv);

	_copyToBackBufferRootSignature.terminate();
	_copyToBackBufferPipelineState.terminate();
	//_pipelineState.terminate();
	//_rootSignature.terminate();

	PipelineStateReloader::Get()->terminate();
	ImGuiSystem::Get()->terminate();

	GpuTimerManager::Get()->terminate();
	DescriptorAllocatorGroup::Get()->terminate();
	_commandListPool.terminate();
	_commandQueue.terminate();
	_swapChain.terminate();

	VramUpdater::Get()->terminate();
	ReleaseQueue::Get()->terminate();
	GlobalVideoMemoryAllocator::Get()->terminate();
	DeviceManager::Get()->terminate();
}

void Renderer::update() {
	RenderDirector::Get()->update();
	GpuTimerManager::Get()->update(_frameIndex);
	VramUpdater::Get()->update(_frameIndex);
	PipelineStateReloader::Get()->update();
	FrameBufferAllocator::Get()->reset();
	FrameDescriptorAllocator::Get()->reset();
	DebugRenderer::Get()->update();
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
	}

	// View テクスチャからバックバッファにコピー
	{
		DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "CopyBackBuffer");
		RenderViewScene* renderViewScene = RenderViewScene::Get();
		GpuTexture* mainViewTexture = renderViewScene->getMainViewTexture();
		GpuTexture* backBuffer = &_backBuffers[_frameIndex];

		ScopedBarrierDesc barriers[] = {
			ScopedBarrierDesc(backBuffer, rhi::RESOURCE_STATE_RENDER_TARGET),
			ScopedBarrierDesc(mainViewTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		};
		ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

		// これはいらんはず。IMGUIのカスタムデスクリプタヒープを何とかする
		rhi::DescriptorHeap* descriptorHeaps[] = { DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->getDescriptorHeap() };
		commandList->setDescriptorHeaps(LTN_COUNTOF(descriptorHeaps), descriptorHeaps);

		commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->setGraphicsRootSignature(&_copyToBackBufferRootSignature);
		commandList->setPipelineState(&_copyToBackBufferPipelineState);
		commandList->setGraphicsRootDescriptorTable(CopyTextureRootParam::INPUT_SRV, renderViewScene->getMainViewGpuSrv());
		commandList->setRenderTargets(1, _backBufferRtv.get(_frameIndex)._cpuHandle, nullptr);
		commandList->drawInstanced(3, 1, 0, 0);

		// 最後にデバッグ描画を乗せる
		ImGuiSystem::Get()->render(commandList);
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
