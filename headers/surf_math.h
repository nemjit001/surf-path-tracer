#pragma once

#define GLM_FORCE_LEFT_HANDED

#include <cfloat>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "surf.h"
#include "types.h"

#define F32_NEG_INF		-INFINITY
#define F32_INF			INFINITY
#define F32_MAX			FLT_MAX
#define F32_MIN			FLT_MIN
#define F32_FAR_AWAY	1e30f
#define F32_EPSILON		1e-5f

#define F32_PI			3.14159265358979323846264f
#define F32_INV_PI		0.31830988618379067153777f
#define F32_INV_2PI		0.15915494309189533576888f
#define F32_2PI			6.28318530717958647692528f

struct ALIGN(8) Float2
{
	union {
		struct { F32 x, y; };
		struct { F32 u, v; };
		F32 xy[2];
	};

	inline Float2() : Float2(0.0f) {};
	inline Float2(F32 val) : Float2(val, val) {};
	inline Float2(F32 x, F32 y) : x(x), y(y) {};

	inline F32 magnitude() const;
	inline F32 dot(const Float2& other) const;
	inline Float2 normalize() const;

	inline F32 operator[](SizeType index) const { return xy[index]; }
	inline operator glm::vec2() const { return glm::vec2(x, y); }
};

struct ALIGN(16) Float3
{
	union {
		struct { F32 x, y, z; };
		struct { F32 r, g, b; };
		F32 xyz[3];
	};

	inline Float3() : Float3(0.0f) {};
	inline Float3(F32 val) : Float3(val, val, val) {};
	inline Float3(const Float2& xy, F32 z) : Float3(xy.x, xy.y, z) {};
	inline Float3(F32 x, F32 y, F32 z) : x(x), y(y), z(z) {};

	inline F32 magnitude() const;
	inline F32 dot(const Float3& other) const;
	inline Float3 normalize() const;
	inline Float3 cross(const Float3& other) const;

	inline F32 operator[](SizeType index) const { return xyz[index]; }
	inline operator glm::vec3() const { return glm::vec3(x, y, z); }
};

struct ALIGN(16) Float4
{
	union {
		struct { F32 x, y, z, w; };
		struct { F32 r, g, b, a; };
		F32 xyzw[4];
	};

	inline Float4() : Float4(0.0f) {};
	inline Float4(F32 val) : Float4(val, val) {};
	inline Float4(const Float3& xyz, F32 w) : Float4(xyz.x, xyz.y, xyz.z, w) {};
	inline Float4(F32 x, F32 y, F32 z, F32 w) : x(x), y(y), z(z), w(w) {};

	inline F32 magnitude() const;
	inline F32 dot(const Float4& other) const;
	inline Float4 normalize() const;

	inline F32 operator[](SizeType index) const { return xyzw[index]; }
	inline operator glm::vec4() const { return glm::vec4(x, y, z, w); }
};

using RgbColor	= Float3;
using RgbaColor = Float4;

using Mat4 = glm::mat4;	// Matrix stuff is hard, so steal it from GLM

template <typename T>
inline void swap(T& a, T& b) { T temp; temp = a; a = b; b = temp; }

inline F32 rsqrtf(const F32 x) { return 1.0f / sqrtf(x); }

inline F32 clamp(F32 a, F32 min, F32 max) { return a < min ? min : (a > max ? max : a); }
inline Float2 clamp(const Float2& a, F32 min, F32 max) { return Float2(clamp(a.x, min, max), clamp(a.y, min, max)); }
inline Float3 clamp(const Float3& a, F32 min, F32 max) { return Float3(clamp(a.x, min, max), clamp(a.y, min, max), clamp(a.z, min, max)); }
inline Float4 clamp(const Float4& a, F32 min, F32 max) { return Float4(clamp(a.x, min, max), clamp(a.y, min, max), clamp(a.z, min, max), clamp(a.w, min, max)); }

inline Float2 expf(const Float2& a) { return Float2(expf(a.x), expf(a.y)); }
inline Float3 expf(const Float3& a) { return Float3(expf(a.x), expf(a.y), expf(a.z)); }
inline Float4 expf(const Float4& a) { return Float4(expf(a.x), expf(a.y), expf(a.z), expf(a.w)); }

inline F32 min(F32 a, F32 b) { return a < b ? a : b; };
inline F32 max(F32 a, F32 b) { return a > b ? a : b; };

inline U32 min(U32 a, U32 b) { return a < b ? a : b; };
inline U32 max(U32 a, U32 b) { return a > b ? a : b; };

inline SizeType min(SizeType a, SizeType b) { return a < b ? a : b; };
inline SizeType max(SizeType a, SizeType b) { return a > b ? a : b; };

inline Float3 min(const Float3& a, const Float3& b) { return Float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)); }
inline Float3 max(const Float3& a, const Float3& b) { return Float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)); }

// Float2 operators
inline Float2 operator+(const Float2& a, F32 b) { return Float2(a.x + b, a.y + b); }
inline Float2 operator-(const Float2& a, F32 b) { return Float2(a.x - b, a.y - b); }
inline Float2 operator*(const Float2& a, F32 b) { return Float2(a.x * b, a.y * b); }
inline Float2 operator/(const Float2& a, F32 b) { return Float2(a.x / b, a.y / b); }

inline Float2 operator+(const Float2& a, const Float2& b) { return Float2(a.x + b.x, a.y + b.y); }
inline Float2 operator-(const Float2& a, const Float2& b) { return Float2(a.x - b.x, a.y - b.y); }
inline Float2 operator*(const Float2& a, const Float2& b) { return Float2(a.x * b.x, a.y * b.y); }
inline Float2 operator/(const Float2& a, const Float2& b) { return Float2(a.x / b.x, a.y / b.y); }

inline void operator+=(Float2& a, F32 b) { a.x += b; a.y += b; }
inline void operator-=(Float2& a, F32 b) { a.x -= b; a.y -= b; }
inline void operator*=(Float2& a, F32 b) { a.x *= b; a.y *= b; }
inline void operator/=(Float2& a, F32 b) { a.x /= b; a.y /= b; }

inline void operator+=(Float2& a, const Float2& b) { a.x += b.x; a.y += b.y; }
inline void operator-=(Float2& a, const Float2& b) { a.x -= b.x; a.y -= b.y; }
inline void operator*=(Float2& a, const Float2& b) { a.x *= b.x; a.y *= b.y; }
inline void operator/=(Float2& a, const Float2& b) { a.x /= b.x; a.y /= b.y; }

inline F32 Float2::magnitude() const { return sqrtf(this->dot(*this)); }
inline F32 Float2::dot(const Float2& other) const { return x * other.x + y * other.y; }
inline Float2 Float2::normalize() const { F32 invLen = rsqrtf(this->dot(*this)); return *this * invLen; }

// Float3 operators
inline Float3 operator+(const Float3& a, F32 b) { return Float3(a.x + b, a.y + b, a.z + b); }
inline Float3 operator-(const Float3& a, F32 b) { return Float3(a.x - b, a.y - b, a.z - b); }
inline Float3 operator*(const Float3& a, F32 b) { return Float3(a.x * b, a.y * b, a.z * b); }
inline Float3 operator/(const Float3& a, F32 b) { return Float3(a.x / b, a.y / b, a.z / b); }

inline Float3 operator+(const Float3& a, const Float3& b) { return Float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Float3 operator-(const Float3& a, const Float3& b) { return Float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Float3 operator*(const Float3& a, const Float3& b) { return Float3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Float3 operator/(const Float3& a, const Float3& b) { return Float3(a.x / b.x, a.y / b.y, a.z / b.z); }

inline void operator+=(Float3& a, F32 b) { a.x += b; a.y += b; a.z += b; }
inline void operator-=(Float3& a, F32 b) { a.x -= b; a.y -= b; a.z -= b; }
inline void operator*=(Float3& a, F32 b) { a.x *= b; a.y *= b; a.z *= b; }
inline void operator/=(Float3& a, F32 b) { a.x /= b; a.y /= b; a.z /= b; }

inline void operator+=(Float3& a, const Float3& b) { a.x += b.x; a.y += b.y; a.z += b.z; }
inline void operator-=(Float3& a, const Float3& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; }
inline void operator*=(Float3& a, const Float3& b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; }
inline void operator/=(Float3& a, const Float3& b) { a.x /= b.x; a.y /= b.y; a.z /= b.z; }

inline F32 Float3::magnitude() const { return sqrtf(this->dot(*this)); }
inline F32 Float3::dot(const Float3& other) const { return x * other.x + y * other.y + z * other.z; }
inline Float3 Float3::normalize() const { F32 invLen = rsqrtf(this->dot(*this)); return *this * invLen; }

inline Float3 Float3::cross(const Float3& other) const
{
	return Float3(
		y * other.z - z * other.y,
		z * other.x - x * other.z,
		x * other.y - y * other.x
	);
}

// Float4 operators
inline Float4 operator+(const Float4& a, F32 b) { return Float4(a.x + b, a.y + b, a.z + b, a.w + b); }
inline Float4 operator-(const Float4& a, F32 b) { return Float4(a.x - b, a.y - b, a.z - b, a.w - b); }
inline Float4 operator*(const Float4& a, F32 b) { return Float4(a.x * b, a.y * b, a.z * b, a.w * b); }
inline Float4 operator/(const Float4& a, F32 b) { return Float4(a.x / b, a.y / b, a.z / b, a.w / b); }

inline Float4 operator+(const Float4& a, const Float4& b) { return Float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline Float4 operator-(const Float4& a, const Float4& b) { return Float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
inline Float4 operator*(const Float4& a, const Float4& b) { return Float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
inline Float4 operator/(const Float4& a, const Float4& b) { return Float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }

inline void operator+=(Float4& a, F32 b) { a.x += b; a.y += b; a.z += b; a.w += b; }
inline void operator-=(Float4& a, F32 b) { a.x -= b; a.y -= b; a.z -= b; a.w -= b; }
inline void operator*=(Float4& a, F32 b) { a.x *= b; a.y *= b; a.z *= b; a.w *= b; }
inline void operator/=(Float4& a, F32 b) { a.x /= b; a.y /= b; a.z /= b; a.w /= b; }

inline void operator+=(Float4& a, const Float4& b) { a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; }
inline void operator-=(Float4& a, const Float4& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; }
inline void operator*=(Float4& a, const Float4& b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; a.w *= b.w; }
inline void operator/=(Float4& a, const Float4& b) { a.x /= b.x; a.y /= b.y; a.z /= b.z; a.w /= b.w; }

inline F32 Float4::magnitude() const { return sqrtf(this->dot(*this)); }
inline F32 Float4::dot(const Float4& other) const { return x * other.x + y * other.y + z * other.z + w * other.w; }
inline Float4 Float4::normalize() const { F32 invLen = rsqrtf(this->dot(*this)); return *this * invLen; }

U32 RgbaToU32(const RgbaColor& color);

U32 initSeed(U32 seed);

U32 randomU32();

U32 randomU32(U32& seed);

F32 randomF32();

F32 randomF32(U32& seed);

F32 randomRange(F32 min, F32 max);

F32 randomRange(U32& seed, F32 min, F32 max);

Float3 randomOnHemisphere(U32& seed, const Float3& normal);

inline Float3 reflect(const Float3& direction, Float3& normal) { return direction - 2.0f * normal.dot(direction) * normal; }

inline bool depthInBounds(F32 depth, F32 maxDepth) { return F32_EPSILON <= depth && depth < maxDepth; }
