#pragma once
#include <Core/Type.h>
#include <Core/Utility.h>
namespace ltn {
template <class T>
class UpdateInfos {
public:
	using Array = const T* const*;

	Array getObjects() const { return _objects; }
	u32 getUpdateCount() const { return _updateCount; }

	void push(T* mesh) {
		LTN_ASSERT(_updateCount < STACK_COUNT_MAX);
		_objects[_updateCount++] = mesh;
	}

	void reset() {
		_updateCount = 0;
	}

	static constexpr u32 STACK_COUNT_MAX = 256;
	T* _objects[STACK_COUNT_MAX];
	u32 _updateCount = 0;
};
}