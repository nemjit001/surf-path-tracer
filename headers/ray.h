#pragma once

#include "surf_math.h"
#include "types.h"

#define UNSET_INDEX (U32)(~0)

struct GPURayState
{
	ALIGN(4) bool inMedium;
	ALIGN(4) bool lastSpecular;
	ALIGN(4) U32 pixelIdx;
};

struct GPURayHit
{
	ALIGN(4) U32 instanceIdx;
	ALIGN(4) U32 primitiveIdx;
	ALIGN(8) Float2 hitCoords;
};

struct GPURay
{
	ALIGN(16) Float3 origin;
	ALIGN(16) Float3 direction;
	ALIGN(4)  F32 depth;
	ALIGN(16) Float3 transmission;
	ALIGN(16) Float3 energy;
	GPURayState state;
	GPURayHit hit;
};

struct GPUShadowRayMetadata
{
	GPURay shadowRay;
	ALIGN(16) Float3 IL;
	ALIGN(16) Float3 LN;
	ALIGN(16) Float3 brdf;
	ALIGN(16) Float3 N;
	ALIGN(4) U32 hitInstanceIdx;
	ALIGN(4) U32 lightInstanceIdx;
};

struct RayMetadata
{
	U32 primitiveIndex		= UNSET_INDEX;
	U32 instanceIndex		= UNSET_INDEX;
	Float2 hitCoordinates	= Float2(0.0f, 0.0f);
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
