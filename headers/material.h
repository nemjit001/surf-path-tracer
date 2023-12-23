#pragma once

#include "surf_math.h"
#include "types.h"

struct Material
{
	RgbColor emittance		= RgbColor(0.0f);
	RgbColor albedo			= RgbColor(0.0f);
	RgbColor absorption 	= RgbColor(0.0f);
	F32 reflectivity		= 0.0f;
	F32 refractivity		= 0.0f;
	F32 indexOfRefraction	= 1.0f;

	inline bool isLight() const { return emittance.r > 0.0f || emittance.g > 0.0f || emittance.b > 0.0f; }
};
