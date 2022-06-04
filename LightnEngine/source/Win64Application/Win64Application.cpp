#include "Win64Application.h"
#include <Core/Type.h>
#include <Renderer/RenderCore/Renderer.h>
#include <Renderer/RenderCore/ImGuiSystem.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/RenderCore/RenderView.h>
#include <Renderer/RenderCore/GpuShader.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/View.h>
#include <RendererScene/Material.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Shader.h>
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

void update(){
	GpuShaderScene::Get()->update();
	RenderViewScene::Get()->update();
	GpuMeshInstanceManager::Get()->update();
	GpuMeshResourceManager::Get()->update();
	GeometryResourceManager::Get()->update();
	GpuMaterialManager::Get()->update();
	Renderer::Get()->update();
	MaterialScene::Get()->lateUpdate();
	MeshInstanceScene::Get()->lateUpdate();
	MeshScene::Get()->lateUpdate();
	ShaderScene::Get()->lateUpdate();
	ViewScene::Get()->lateUpdate();
}

void render(){
	Renderer::Get()->render();
}
}

void Win64Application::initialize() {
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
	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
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
	Renderer::Get()->waitForIdle();
	update();
}

void Win64Application::run() {
	EditorCamera editorCamera;
	editorCamera.initialize();

	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		editorCamera.update();
		update();
		render();
	}

	editorCamera.terminate();
}
}
}