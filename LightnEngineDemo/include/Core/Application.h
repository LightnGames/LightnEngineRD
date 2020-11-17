#pragma once
#include <Core/CoreModuleSettings.h>
#include <Core/System.h>
#include <functional>

using ApplicationCallBack = std::function<void(u32, u64, s64)>;

struct ApplicationDesc {
};

class LTN_APP_API Application {
public:
	virtual void initialize() = 0;
	virtual void run() = 0;
	virtual void terminate() = 0;
	virtual s32* getWindowHandle() = 0;
	virtual u32 getScreenWidth() const = 0;
	virtual u32 getScreenHeight() const = 0;

	virtual void registApplicationCallBack(ApplicationCallBack& callback) = 0;
};

class LTN_APP_API ApplicationSystem {
public:
	virtual void initialize() = 0;
	virtual void terminate() = 0;
	virtual Application* getApplication() = 0;

	static ApplicationSystem* Get();
};