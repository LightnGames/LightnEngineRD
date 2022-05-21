#pragma once
#include <Core/Math.h>
#include <Core/Type.h>
namespace ltn {
struct Camera {
	Matrix _worldMatrix;
};

class View {
public:
	void initialize(u8* updateFlags){
		_updateFlags = updateFlags;
	}

private:
	Matrix _projectionMatrix;
	u8* _updateFlags = nullptr;
};

class ViewScene {
public:
	static constexpr u32 VIEW_COUNT_MAX = 8;

	void initialize();
	void terminate();
	void lateUpdate();

	const View* getView(u32 index) const { return &_views[index]; }
	const u8* getViewUpdateFlags() const { return _viewUpdatedFlags; }

	static ViewScene* Get();

private:
	void resetUpdateFlags();

private:
	View* _views = nullptr;
	u8* _viewUpdatedFlags = nullptr;
};
}