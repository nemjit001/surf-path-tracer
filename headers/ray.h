#pragma once

#include "surf_math.h"
#include "types.h"

#define UNSET_INDEX (U32)(~0)

struct RayMetadata
{
	U32 primitiveIndex;
	U32 instanceIndex;
	F32 u;
	F32 v;
};

struct Ray
{
	Float3 origin;
	F32 depth;
	Float3 direction;
	RayMetadata metadata;

	Ray(Float3 origin, Float3 direction);

	inline Float3 hitPosition();
};

Float3 Ray::hitPosition()
{
	return origin + depth * direction;
}
