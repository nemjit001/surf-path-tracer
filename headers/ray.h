#pragma once

#include "surf_math.h"
#include "types.h"

#define UNSET_INDEX (U32)(~0)

struct RayMetadata
{
	U32 primitiveIndex		= UNSET_INDEX;
	U32 instanceIndex		= UNSET_INDEX;
	Float2 hitCoordinates	= Float2(0.0f, 0.0f);
};

/// @brief Mirrors GLSL compute struct for Rays -> needed for size calculation on GPU side
struct GPURay
{
	Float3 origin;
	Float3 direction;
	F32 depth;
	Float3 transmission;
	U32 pixelIdx;
	U32 primitiveIdx;
	Float2 hitCoords;
};

struct Ray
{
	Float3 origin;
	F32 depth;
	Float3 direction;
	bool inMedium;
	RayMetadata metadata;

	Ray(Float3 origin, Float3 direction);

	inline Float3 hitPosition();
};

Float3 Ray::hitPosition()
{
	return origin + depth * direction;
}
