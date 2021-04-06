#include "ApplicationImpl.h"

ApplicationSystemImpl _applicationSystem;
InputSystemImpl _inputSystem;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ApplicationImpl* app = static_cast<ApplicationImpl*>(ApplicationSystemImpl::Get()->getApplication());
	app->translateApplicationCallback(message, wParam, lParam);

	switch (message) {
	case WM_PAINT:
		_inputSystem.update();
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

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
	MSG msg = {};
	while (msg.message != WM_QUIT) {
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

bool InputSystemImpl::getKey(KeyCode keyCode) {
	return isKeyDown(_keyStates[keyCode]);
}

bool InputSystemImpl::getKeyDown(KeyCode keyCode) {
	return isKeyDown(_keyDowns[keyCode]);
}

bool InputSystemImpl::getKeyUp(KeyCode keyCode) {
	return isKeyDown(_keyUps[keyCode]);
}

void InputSystemImpl::update() {
	u8 prevKeyState[256];
	memcpy(prevKeyState, _keyStates, sizeof(prevKeyState));

	GetKeyboardState(_keyStates);
	for (u16 i = 0; i < LTN_COUNTOF(prevKeyState); ++i) {
		_keyDowns[i] = (!isKeyDown(prevKeyState[i])) & isKeyDown(_keyStates[i]);
		_keyUps[i] = isKeyDown(prevKeyState[i]) & (!isKeyDown(_keyStates[i]));
	}
}

InputSystem* InputSystem::Get() {
	return &_inputSystem;
}

InputSystemImpl* InputSystemImpl::Get() {
	return &_inputSystem;
}
