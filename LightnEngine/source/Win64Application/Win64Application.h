#pragma once
#include <Core/ModuleSettings.h>
#include <Application/Application.h>
#include <functional>

namespace ltn {
namespace win64app {
using ApplicationCallBack = std::function<void(u32, u64, s64)>;
class Win64Application :public Application {
public:
	void initialize() override;
	void terminate() override;
	void run() override;

	u32 getScreenWidth() override { return _screenWidth; }
	u32 getScreenHeight() override { return _screenHeight; }
	s32* getWindowHandle() override { return _windowHandle; }

private:
	u32 _screenWidth = 0;
	u32 _screenHeight = 0;
	s32* _windowHandle = nullptr;
};
}
}