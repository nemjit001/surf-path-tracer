#pragma once

#define GLM_FORCE_LEFT_HANDED

#include <glm/glm.hpp>
#include <cmath>

#include "types.h"

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
