#pragma once

#include "surf_math.h"

struct Material
{
	RgbColor emittance;
	RgbColor albedo;

	inline bool isLight() const { return emittance.r > 0.0f || emittance.g > 0.0f || emittance.b > 0.0f; }
};
