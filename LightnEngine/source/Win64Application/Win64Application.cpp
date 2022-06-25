#include "Win64Application.h"
#include <Core/Type.h>
#include <Core/CpuTimerManager.h>
#include <Renderer/RenderCore/ImGuiSystem.h>
#include <Windows.h>

namespace ltn {
namespace win64app {
namespace {
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam,
	LPARAM lParam) {
	if (ImGuiSystem::Get()->translateWndProc(hWnd, message, wParam, lParam)) {
		return true;
	}

	switch (message) {
	case WM_PAINT:
		return false;
	case WM_MOUSEMOVE:
		return false;
	case WM_DESTROY:
		PostQuitMessage(0);
		return false;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
}

void Win64Application::initialize() {
	CpuScopedPerf scopedPerf("Win64Application");
	HINSTANCE windowHandle = GetModuleHandle(NULL);

	// Initialize the window class.
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = windowHandle;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"LightnEngine";
	RegisterClassEx(&windowClass);

	u32 width = 1920;
	u32 height = 1080;
	RECT windowRect = { 0, 0, LONG(width), LONG(height) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	HWND hwnd = CreateWindow(windowClass.lpszClassName, windowClass.lpszClassName,
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,  // We have no parent window.
		nullptr,  // We aren't using menus.
		windowHandle, nullptr);

	ShowWindow(hwnd, SW_SHOWDEFAULT);

	_windowHandle = reinterpret_cast<s32*>(hwnd);
	_screenWidth = width;
	_screenHeight = height;
}

void Win64Application::terminate() {
}

bool Win64Application::update() {
	MSG msg = {};
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (msg.message == WM_QUIT) {
		return false;
	}

	return true;
}
}
}