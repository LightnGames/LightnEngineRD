#include "ApplicationImpl.h"

ApplicationSystemImpl _applicationSystem;
InputSystemImpl _inputSystem;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ApplicationImpl* app = static_cast<ApplicationImpl*>(ApplicationSystemImpl::Get()->getApplication());
	app->translateApplicationCallback(message, wParam, lParam);

	switch (message) {
	case WM_CREATE:
		return 0;

	case WM_KEYDOWN:
		return 0;

	case WM_KEYUP:
		return 0;

	case WM_PAINT:
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void ApplicationImpl::initialize() {
	HINSTANCE windowHandle = GetModuleHandle(NULL);

	// Initialize the window class.
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = windowHandle;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = "LightnEngine";
	RegisterClassEx(&windowClass);

	u32 width = 1920;
	u32 height = 1080;
	RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	HWND hwnd = CreateWindow(
		windowClass.lpszClassName,
		windowClass.lpszClassName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,        // We have no parent window.
		nullptr,        // We aren't using menus.
		windowHandle,
		nullptr);

	ShowWindow(hwnd, SW_SHOW);
	_hWnd = hwnd;
	_screenWidth = width;
	_screenHeight = height;
}

void ApplicationImpl::run() {
	// Main sample loop.
	MSG msg = {};
	while (msg.message != WM_QUIT) {
		// Process any messages in the queue.
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void ApplicationImpl::terminate() {
}

void ApplicationImpl::translateApplicationCallback(u32 message, u64 wParam, s64 lParam) {
	u32 callbackCount = _callbacks.getCount();
	for (u32 callbackIndex = 0; callbackIndex < callbackCount; ++callbackIndex) {
		_callbacks[callbackIndex](message, wParam, lParam);
	}
}

void ApplicationImpl::registApplicationCallBack(ApplicationCallBack& callback) {
	_callbacks.push(callback);
}

ApplicationSystem* ApplicationSystem::Get(){
	return &_applicationSystem;
}

void ApplicationSystemImpl::initialize() {
}

void ApplicationSystemImpl::terminate() {
}

ApplicationSystemImpl* ApplicationSystemImpl::Get() {
	return &_applicationSystem;
}

void InputSystemImpl::initialize() {
}

void InputSystemImpl::terminate() {
}

InputSystem* InputSystem::Get() {
	return &_inputSystem;
}

InputSystemImpl* InputSystemImpl::Get() {
	return &_inputSystem;
}
