#include "ImGuiSystem.h"
#include <ThiredParty/ImGui/imgui_impl_dx12.h>
#include <ThiredParty/ImGui/imgui_impl_win32.h>
#include <Renderer/RenderCore/RendererUtility.h>

#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GpuTimerManager.h>
#include <Core/CpuTimerManager.h>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ltn {
namespace{
ImGuiSystem g_imGuiSystem;
}

void ImGuiSystem::initialize(const Desc& desc) {
	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	rhi::DescriptorHeapDesc allocatorDesc = {};
	allocatorDesc._device = desc._device;
	allocatorDesc._numDescriptors = desc._descriptorCount;
	allocatorDesc._type = rhi::DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	allocatorDesc._flags = rhi::DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_descriptorHeap.initialize(allocatorDesc);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(desc._windowHandle);
	ImGui_ImplDX12_Init(desc._device->_device, rhi::BACK_BUFFER_COUNT,
		rhi::toD3d12(rhi::BACK_BUFFER_FORMAT), _descriptorHeap._descriptorHeap,
		_descriptorHeap._descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		_descriptorHeap._descriptorHeap->GetGPUDescriptorHandleForHeapStart());

	beginFrame();
}

void ImGuiSystem::terminate() {
	_descriptorHeap.terminate();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiSystem::beginFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// メインビューポートにドッキング可能エリアを追加
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	bool open = true;
	ImGui::ShowDemoWindow(&open);

	ImGui::Begin("TestInfo");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	
	rhi::QueryVideoMemoryInfo videoMemoryInfo = DeviceManager::Get()->getHardwareAdapter()->queryVideoMemoryInfo();
	ImGui::Text("[VMEM info] %-14s / %-14s byte", ThreeDigiets(videoMemoryInfo._currentUsage).get(), ThreeDigiets(videoMemoryInfo._budget).get());
	
	{
		ImGui::Separator();
		GpuTimerManager* timerManager = GpuTimerManager::Get();
		const u64* timeStamps = GpuTimeStampManager::Get()->getGpuTimeStamps();
		f64 gpuTickDelta = GpuTimeStampManager::Get()->getGpuTickDeltaInMilliseconds();
		u32 timerCount = timerManager->getTimerCount();
		for (u32 i = 0; i < timerCount; ++i) {
			u32 offset = i * 2;
			f64 time = (timeStamps[offset + 1] - timeStamps[offset]) * gpuTickDelta;
			ImGui::Text("%-20s %2.3f ms", timerManager->getGpuTimerAdditionalInfo(i)->_name, time);
		}
	}

	{
		ImGui::Separator();
		CpuTimerManager* timerManager = CpuTimerManager::Get();
		const u64* timeStamps = CpuTimeStampManager::Get()->getCpuTimeStamps();
		f64 gpuTickDelta = CpuTimeStampManager::Get()->getCpuTickDeltaInMilliseconds();
		u32 timerCount = timerManager->getTimerCount();
		for (u32 i = 0; i < timerCount; ++i) {
			u32 offset = i * 2;
			f64 time = (timeStamps[offset + 1] - timeStamps[offset]) * gpuTickDelta;
			ImGui::Text("%-20s %2.3f ms", timerManager->getGpuTimerAdditionalInfo(i)->_name, time);
		}
	}

	ImGui::End();
}

void ImGuiSystem::render(rhi::CommandList* commandList) {
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "ImGui");
	ImGui::Render();

	rhi::DescriptorHeap* descriptorHeaps[] = { &_descriptorHeap };
	commandList->setDescriptorHeaps(1, descriptorHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList->_commandList);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, commandList->_commandList);
	}

	ImGuiSystem::Get()->beginFrame();
}

bool ImGuiSystem::translateWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
}

ImGuiSystem* ImGuiSystem::Get() {
	return &g_imGuiSystem;
}
}