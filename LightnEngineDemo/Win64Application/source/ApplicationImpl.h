#pragma once
#include <Core/Application.h>
#include <Windows.h>

class ApplicationImpl :public Application {
public:
	static constexpr u32 APPLICATION_CALLBACK_COUNT_MAX = 32;

	void initialize() override;
	void run() override;
	void terminate() override;
	virtual s32* getWindowHandle() override {
		return reinterpret_cast<s32*>(_hWnd);
	}
	virtual u32 getScreenWidth() const override {
		return _screenWidth;
	}
	virtual u32 getScreenHeight() const override {
		return _screenHeight;
	}

	void translateApplicationCallback(u32 message, u64 wParam, s64 lParam);
	void registApplicationCallBack(ApplicationCallBack& callback) override;

private:
	HWND _hWnd = nullptr;
	u32 _screenWidth = 0;
	u32 _screenHeight = 0;
	LinerArray<ApplicationCallBack, APPLICATION_CALLBACK_COUNT_MAX> _callbacks;
};

class ApplicationSystemImpl :public ApplicationSystem {
public:
	virtual void initialize() override;
	virtual void terminate() override;
	virtual Application* getApplication() override { return &_application; };

	static ApplicationSystemImpl* Get();

private:
	ApplicationImpl _application;
};
