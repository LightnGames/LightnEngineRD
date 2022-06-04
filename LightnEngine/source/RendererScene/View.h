#pragma once
#include <Core/Math.h>
#include <Core/Type.h>
namespace ltn {
struct Camera {
	Matrix4 _worldMatrix;
	f32 _fov = 1.0472f;
	f32 _farClip = 300;
	f32 _nearClip = 0.1f;
};

class View {
public:
	void setWidth(u32 width) { _width = width; }
	void setHeight(u32 height) { _height = height; }

	u32 getWidth() const { return _width; }
	u32 getHeight() const { return _height; }
	f32 getAspectRate() const { return f32(_width) / f32(_height); }
	Camera* getCamera() { return &_camera; }
	const Camera* getCamera() const { return &_camera; }

private:
	u32 _width = 0;
	u32 _height = 0;
	Camera _camera;
};

class ViewScene {
public:
	static constexpr u32 VIEW_COUNT_MAX = 8;

	enum ViewStateFlags {
		VIEW_STATE_NONE = 0,
		VIEW_STATE_CREATED = 1 << 0,
		VIEW_STATE_UPDATED = 1 << 1,
		VIEW_STATE_DESTROY = 1 << 2
	};

	void initialize();
	void terminate();
	void lateUpdate();

	View* createView();
	void destroyView(View* view);

	u32 getViewIndex(const View* view) const { return u32(view - _views); }
	const View* getView(u32 index) const { return &_views[index]; }
	const u8* getViewStateFlags() const { return _viewStateFlags; }
	const u8* getViewEnabledFlags() const { return _viewEnabledFlags; }

	static ViewScene* Get();

private:
	View* _views = nullptr;
	u8* _viewStateFlags = nullptr;
	u8* _viewEnabledFlags = nullptr;
};
}