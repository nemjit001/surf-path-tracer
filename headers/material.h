#pragma once

#include "surf_math.h"
#include "types.h"

struct Material
{
	RgbColor emissionColor = RgbColor(0.0f);
	RgbColor baseColor = RgbColor(0.0f);
	RgbColor absorption = RgbColor(0.0f);
	F32 emissionStrength 			= 0.0f;
	F32 refractance					= 0.0f;
	F32 indexOfRefraction			= 1.0f;
	F32 metallic					= 0.0f;
	F32 specular					= 0.5f;
	F32 roughness					= 0.5f;
	F32 reflectance					= 0.0f;

	inline bool isLight() const { return emissionStrength > 0.0f && (emissionColor.r > 0.0f || emissionColor.g > 0.0f || emissionColor.b > 0.0f); }

	inline RgbColor emittance() const { return emissionStrength * emissionColor; }
};
