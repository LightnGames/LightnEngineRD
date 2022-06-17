#include "View.h"
#include <string.h>
#include <Core/Memory.h>
#include <Core/Utility.h>

namespace ltn {
namespace {
ViewScene g_viewScene;
}

void ViewScene::initialize() {
	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_views = allocation.allocateClearedObjects<View>(VIEW_COUNT_MAX);
		_viewStateFlags = allocation.allocateClearedObjects<u8>(VIEW_COUNT_MAX);
		_viewEnabledFlags = allocation.allocateClearedObjects<u8>(VIEW_COUNT_MAX);
		});
}

void ViewScene::terminate() {
	lateUpdate();
	for (u32 i = 0; i < VIEW_COUNT_MAX; ++i) {
		LTN_ASSERT(_viewEnabledFlags[i] == 0);
	}

	_chunkAllocator.free();
}

void ViewScene::lateUpdate() {
	for (u32 i = 0; i < VIEW_COUNT_MAX; ++i) {
		if (_viewStateFlags[i] == View::VIEW_STATE_DESTROY) {
			_viewEnabledFlags[i] = 0;
			_views[i] = View();
		}
	}

	memset(_viewStateFlags, 0, sizeof(u8) * VIEW_COUNT_MAX);
}

View* ViewScene::createView() {
	for (u32 i = 0; i < VIEW_COUNT_MAX; ++i) {
		if (_viewEnabledFlags[i] == 0) {
			_viewEnabledFlags[i] = 1;
			_viewStateFlags[i] |= View::VIEW_STATE_CREATED;
			_views[i].setStateFlags(&_viewStateFlags[i]);
			return &_views[i];
		}
	}

	return nullptr;
}

void ViewScene::destroyView(View* view) {
	u32 viewIndex = getViewIndex(view);
	LTN_ASSERT(viewIndex < VIEW_COUNT_MAX);

	_viewStateFlags[viewIndex] = View::VIEW_STATE_DESTROY;
}

ViewScene* ViewScene::Get() {
	return &g_viewScene;
}
}
