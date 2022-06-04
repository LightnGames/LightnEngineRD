#pragma once
#include <Core/Type.h>
#include <Core/ModuleSettings.h>
#include <Editor/EditorCamera.h>
namespace ltn {
class Application {
public:
	virtual void initialize() = 0;
	virtual void terminate() = 0;
	virtual void run() = 0;

	virtual u32 getScreenWidth() = 0;
	virtual u32 getScreenHeight() = 0;
	virtual s32* getWindowHandle() = 0;
};

class LTN_API ApplicationSysytem {
public:
	void setApplication(Application* app) {
		_application = app;
	}

	Application* getApplication() {
		return _application;
	}

	static ApplicationSysytem* Get();

private:
	Application* _application = nullptr;
};
}