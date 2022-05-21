#include "View.h"
#include <string.h>
#include <Core/Memory.h>

namespace ltn {
namespace {
ViewScene g_viewScene;
}

void ViewScene::initialize() {
	_views = Memory::allocObjects<View>(VIEW_COUNT_MAX);
	_viewUpdatedFlags = Memory::allocObjects<u8>(VIEW_COUNT_MAX);
	for (u32 i = 0; i < VIEW_COUNT_MAX; ++i) {
		_views[i].initialize(&_viewUpdatedFlags[i]);
	}

	resetUpdateFlags();
}

void ViewScene::terminate() {
	Memory::freeObjects(_views);
	Memory::freeObjects(_viewUpdatedFlags);
}

void ViewScene::lateUpdate() {
	resetUpdateFlags();
}

ViewScene* ViewScene::Get() {
	return &g_viewScene;
}

void ViewScene::resetUpdateFlags() {
	memset(_viewUpdatedFlags, 0, sizeof(u8) * VIEW_COUNT_MAX);
}
}
