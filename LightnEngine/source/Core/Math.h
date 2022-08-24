#pragma once
#include <Core/Type.h>
#include <DirectXMath.h>

#define NOMINMAX
#undef min
#undef max

// DirectX Math ÇÃÉâÉbÉpÅ[
namespace ltn {
using Float4x4 = DirectX::XMFLOAT4X4;
using Float4x3 = DirectX::XMFLOAT4X3;
using Float3x4 = DirectX::XMFLOAT3X4;
using Float2 = DirectX::XMFLOAT2;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;
using Color4 = Float4;

using Vector = DirectX::XMVECTORF32;
using Matrix = DirectX::XMMATRIX;

static f32 DegToRad(f32 degree) {
	return DirectX::XMConvertToRadians(degree);
}

static f32 Tan(f32 value) {
	return tanf(value);
}

struct Vector2 : public Vector {
	Vector2() {
		v = DirectX::XMVectorZero();
	}

	Vector2(DirectX::XMVECTOR v1) {
		v = v1;
	}

	Vector2(Float2 vector) {
		v = DirectX::XMLoadFloat2(&vector);
	}

	Vector2(f32 x, f32 y) {
		v = DirectX::XMVectorSet(x, y, 0.0f, 1.0f);
	}

	Vector2 operator +() const {
		return v;
	}

	Vector2 operator -() const {
		return DirectX::XMVectorNegate(v);
	}

	Vector2 operator *(f32 s) const {
		return DirectX::XMVectorScale(*this, s);
	}

	Vector2 operator /(f32 s) const {
		DirectX::XMVECTOR vS = DirectX::XMVectorReplicate(s);
		return DirectX::XMVectorDivide(*this, vS);
	}

	Vector2 operator +(const Vector2& v) const {
		return DirectX::XMVectorAdd(*this, v);
	}

	Vector2 operator -(const Vector2& v) const {
		return DirectX::XMVectorSubtract(*this, v);
	}

	f32 getX() const {
		return DirectX::XMVectorGetX(*this);
	}

	f32 getY() const {
		return DirectX::XMVectorGetY(*this);
	}
};

struct Vector3 : public Vector {
	Vector3() {
		v = DirectX::XMVectorZero();
	}

	Vector3(DirectX::XMVECTOR v1) {
		v = v1;
	}

	Vector3(Float3 vector) {
		v = DirectX::XMLoadFloat3(&vector);
	}

	Vector3(f32 x, f32 y, f32 z) {
		v = DirectX::XMVectorSet(x, y, z, 1.0f);
	}

	Vector3 operator +() const {
		return v;
	}

	Vector3 operator -() const {
		return DirectX::XMVectorNegate(v);
	}

	Vector3 operator *(f32 s) const {
		return DirectX::XMVectorScale(*this, s);
	}

	Vector3 operator /(f32 s) const {
		DirectX::XMVECTOR vS = DirectX::XMVectorReplicate(s);
		return DirectX::XMVectorDivide(*this, vS);
	}

	Vector3 operator +(const Vector3& v) const {
		return DirectX::XMVectorAdd(*this, v);
	}

	Vector3 operator -(const Vector3& v) const {
		return DirectX::XMVectorSubtract(*this, v);
	}

	Vector3 operator *=(f32 s) {
		*this = *this * s;
		return v;
	}

	Vector3 operator /=(f32 s) {
		*this = *this / s;
		return v;
	}

	Vector3 operator +=(const Vector3& v1) {
		v = v + v1;
		return v;
	}

	Vector3 operator -=(const Vector3& v1) {
		v = v - v1;
		return v;
	}

	f32 getX() const {
		return DirectX::XMVectorGetX(v);
	}

	f32 getY() const {
		return DirectX::XMVectorGetY(v);
	}

	f32 getZ() const {
		return DirectX::XMVectorGetZ(v);
	}

	Vector3 multiply(const Vector3& v1) const {
		return DirectX::XMVectorMultiply(v, v1);
	}

	f32 length() const {
		return DirectX::XMVectorGetX(DirectX::XMVector3Length(v));
	}

	Vector3 normalize() const {
		return DirectX::XMVector3Normalize(v);
	}

	Float3 getFloat3() const {
		Float3 result;
		DirectX::XMStoreFloat3(&result, v);
		return result;
	}

	Vector3 min(const Vector3& v1) const {
		return DirectX::XMVectorMin(v, v1);
	}

	Vector3 max(const Vector3& v1) const {
		return DirectX::XMVectorMax(v, v1);
	}

	static f32 Dot(const Vector3& v1, const Vector3& v2) {
		return DirectX::XMVectorGetX(DirectX::XMVector3Dot(v1, v2));
	}

	static Vector3 Zero() {
		return DirectX::g_XMZero.v;
	}

	static Vector3 One() {
		return DirectX::g_XMOne3.v;
	}

	static Vector3 Right() {
		return DirectX::g_XMIdentityR0.v;
	}

	static Vector3 Up() {
		return DirectX::g_XMIdentityR1.v;
	}

	static Vector3 Forward() {
		return DirectX::g_XMIdentityR2.v;
	}

	static Vector3 FltMax() {
		return DirectX::g_XMFltMax.v;
	}
};

static Float4 MakeFloat4(const Vector3& v, f32 w) {
	Float3 f = v.getFloat3();
	return Float4(f.x, f.y, f.z, w);
}

struct Vector4 :public Vector {
	Vector4() {
		v = DirectX::XMVectorZero();
	}

	Vector4(DirectX::XMVECTOR v1) {
		v = v1;
	}

	Vector4(f32 x, f32 y, f32 z, f32 w) {
		v = DirectX::XMVectorSet(x, y, z, w);
	}

	Vector3 getVector3() const {
		return v;
	}

	Float3 getFloat3() const {
		Float3 result;
		DirectX::XMStoreFloat3(&result, v);
		return result;
	}

	Float4 getFloat4() const {
		Float4 result;
		DirectX::XMStoreFloat4(&result, v);
		return result;
	}
};

struct Matrix4 {
	Matrix4(const DirectX::XMMATRIX matrix) :_m(matrix) {}

	Matrix4() {
		_m = DirectX::XMMatrixSet(
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f);
	}
	Matrix4(
		f32 m11, f32 m12, f32 m13, f32 m14,
		f32 m21, f32 m22, f32 m23, f32 m24,
		f32 m31, f32 m32, f32 m33, f32 m34,
		f32 m41, f32 m42, f32 m43, f32 m44) {
		_m = DirectX::XMMatrixSet(
			m11, m12, m13, m14,
			m21, m22, m23, m24,
			m31, m32, m33, m34,
			m41, m42, m43, m44);
	}

	Matrix4 transpose() const {
		return DirectX::XMMatrixTranspose(_m);
	}

	Matrix4 inverse() const {
		return DirectX::XMMatrixInverse(nullptr, _m);
	}

	Float4x4 getFloat4x4() const {
		Float4x4 result;
		DirectX::XMStoreFloat4x4(&result, _m);
		return result;
	}

	Float4x3 getFloat4x3() const {
		Float4x3 result;
		DirectX::XMStoreFloat4x3(&result, _m);
		return result;
	}

	Float3x4 getFloat3x4() const {
		Float3x4 result;
		DirectX::XMStoreFloat3x4(&result, _m);
		return result;
	}

	Vector4 getCol(u32 index) const {
		return _m.r[index];
	}

	Vector3 getTranslation() const {
		return getCol(3).getVector3();
	}

	static Matrix4 identity() {
		return DirectX::XMMatrixIdentity();
	}

	static Matrix4 rotationX(f32 angle) {
		return DirectX::XMMatrixRotationX(angle);
	}

	static Matrix4 rotationY(f32 angle) {
		return DirectX::XMMatrixRotationY(angle);
	}

	static Matrix4 rotationZ(f32 angle) {
		return DirectX::XMMatrixRotationZ(angle);
	}

	static Matrix4 rotationXYZ(f32 pitch, f32 yaw, f32 roll) {
		return DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
	}

	static Matrix4 translationFromVector(const Vector3& offset) {
		return DirectX::XMMatrixTranslationFromVector(offset.v);
	}

	static Matrix4 perspectiveFovLH(f32 fovAngleY, f32 aspectHByW, f32 nearZ, f32 farZ) {
		return DirectX::XMMatrixPerspectiveFovLH(fovAngleY, aspectHByW, nearZ, farZ);
	}

	static Vector3 transform(const Vector3& v, const Matrix4& m) {
		return DirectX::XMVector3Transform(v.v, m._m);
	}

	static Vector3 transformNormal(const Vector3& v, const Matrix4& m) {
		return DirectX::XMVector3TransformNormal(v.v, m._m);
	}

	Matrix4 operator *(const Matrix4& m1) const {
		return DirectX::XMMatrixMultiply(_m, m1._m);
	}

	Matrix _m;
};
}