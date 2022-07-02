#pragma once
#include "Math.h"

namespace ltn {
struct AABB {
	AABB() {}
	AABB(const Vector3& min, const Vector3& max) :_min(min), _max(max) {}

	AABB getTransformedAabb(const Matrix4& matrix) {
		Vector3 min = _min;
		Vector3 size = getSize();

		const Vector3 points[8] = {
			min + size.multiply(Vector3(1, 1, 1)),
			min + size.multiply(Vector3(1, 1, 0)),
			min + size.multiply(Vector3(1, 0, 0)),
			min,
			min + size.multiply(Vector3(1, 0, 1)),
			min + size.multiply(Vector3(0, 0, 1)),
			min + size.multiply(Vector3(0, 1, 1)),
			min + size.multiply(Vector3(0, 1, 0)), };

		Vector3 resultMin = Vector3::FltMax();
		Vector3 resultMax = -Vector3::FltMax();

		for (u8 i = 0; i < 8; ++i) {
			Vector3 p = Matrix4::transform(points[i], matrix);
			resultMin = p.min(resultMin);
			resultMax = p.max(resultMax);
		}

		return AABB(resultMin, resultMax);
	}

	Vector3 getSize() const {
		return (_max - _min);
	}

	Vector3 getHalfSize() const {
		return getSize() / 2.0f;
	}

	Vector3 getCenter() const {
		return _min + getHalfSize();
	}

	Vector3 _min;
	Vector3 _max;
};
}