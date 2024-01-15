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

struct GPURayState
{
	bool inMedium;
	bool lastSpecular;
	uint pixelIdx;
};

struct GPUShadowRayState
{
	uint pixelIdx;
	uint instanceIdx;
	uint primitiveIdx;
	Float2 triCoords;
	Float3 I;
	Float3 N;
	Float3 brdf;
};

struct GPURayHit
{
	U32 instanceIdx;
	U32 primitiveIdx;
	Float2 hitCoords;
};

struct GPUShadowRay
{
	Float3 origin;
	Float3 direction;
	F32 depth;
	GPUShadowRayState state;
};

struct GPURay
{
	Float3 origin;
	Float3 direction;
	F32 depth;
	Float3 transmission;
	Float3 energy;
	GPURayState state;
	GPURayHit hit;
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
