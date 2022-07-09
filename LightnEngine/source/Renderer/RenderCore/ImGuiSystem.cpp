#include "ImGuiSystem.h"
#include <ThiredParty/ImGui/imgui_impl_dx12.h>
#include <ThiredParty/ImGui/imgui_impl_win32.h>
#include <Renderer/RenderCore/RendererUtility.h>

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

#if 0
	bool open = true;
	ImGui::ShowDemoWindow(&open);
#endif
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