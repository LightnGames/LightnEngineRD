#pragma once
#include <Core/ModuleSettings.h>
namespace ltn {
class View;
class LTN_API EditorCamera {
public:
	void initialize();
	void terminate();
	void update();
private:
	View* _view = nullptr;
};
}