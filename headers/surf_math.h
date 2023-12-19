#pragma once

#define GLM_FORCE_LEFT_HANDED

#include <glm/glm.hpp>
#include <cmath>

#include "types.h"

#define F32_INFINITY	INFINITY
#define F32_EPSILON		1e-5

using Float2 = glm::vec2;
using Float3 = glm::vec3;
using Float4 = glm::vec4;

using RgbColor	= glm::vec3;
using RgbaColor = glm::vec4;

U32 RgbaToU32(RgbaColor color);
