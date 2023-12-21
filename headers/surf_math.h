#pragma once

#define GLM_FORCE_LEFT_HANDED

#include <glm/glm.hpp>
#include <cmath>

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

using Float2 = glm::vec2;
using Float3 = glm::vec3;
using Float4 = glm::vec4;

using RgbColor	= glm::vec3;
using RgbaColor = glm::vec4;

U32 RgbaToU32(const RgbaColor& color);

U32 randomU32();

U32 randomU32(U32& seed);

F32 randomF32();

F32 randomF32(U32& seed);

F32 randomRange(F32 range);

F32 randomRange(U32& seed, F32 range);

Float3 randomOnHemisphere(const Float3& normal);

bool depthInBounds(F32 depth, F32 maxDepth);

template <typename T>
inline void swap(T& a, T& b) { T temp; temp = a; a = b; b = temp; }

inline F32 min(F32 a, F32 b) { return a < b ? a : b; };
inline F32 max(F32 a, F32 b) { return a > b ? a : b; };

inline U32 min(U32 a, U32 b) { return a < b ? a : b; };
inline U32 max(U32 a, U32 b) { return a > b ? a : b; };

inline SizeType min(SizeType a, SizeType b) { return a < b ? a : b; };
inline SizeType max(SizeType a, SizeType b) { return a > b ? a : b; };

inline Float3 min(const Float3& a, const Float3& b) { return Float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)); }
inline Float3 max(const Float3& a, const Float3& b) { return Float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)); }
