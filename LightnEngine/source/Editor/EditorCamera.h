#pragma once
#include <Core/ModuleSettings.h>

namespace ltn {
class View;
class DirectionalLight;
class EditorCamera {
public:
	void initialize();
	void terminate();
	void update();

private:
	View* _view = nullptr;
	DirectionalLight* _directionalLight = nullptr;
};
}