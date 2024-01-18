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
	U32 pixelIdx;
};

struct GPURayHit
{
	U32 instanceIdx;
	U32 primitiveIdx;
	Float2 hitCoords;
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

struct GPUShadowRayMetadata
{
	GPURay shadowRay;
	Float3 IL;
	Float3 LN;
	Float3 brdf;
	Float3 N;
	ALIGN(16) U32 hitInstanceIdx;
	U32 lightInstanceIdx;
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
