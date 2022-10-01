#pragma once
#include <Renderer/RHI/Rhi.h>
#include <ThiredParty/ImGui/imgui.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
class ImGuiSystem {
public:
	struct Desc {
		s32* _windowHandle = nullptr;
		rhi::Device* _device = nullptr;
		u32 _descriptorCount = 0;
	};

	void initialize(const Desc& desc);
	void terminate();
	void beginFrame();
	void render(rhi::CommandList* commandList);
	bool translateWndProc(HWND hWnd, UINT message, WPARAM wParam,
		LPARAM lParam);

	static ImGuiSystem* Get();

private:
	rhi::DescriptorHeap _descriptorHeap;
};
}