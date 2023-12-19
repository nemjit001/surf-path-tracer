#pragma once

#include "surf_math.h"
#include "types.h"

struct Ray
{
	Float3 origin;
	F32 depth;
	Float3 direction;

	Ray(Float3 origin, Float3 direction);

	inline Float3 hitPosition();
};

Float3 Ray::hitPosition()
{
	return origin + depth * direction;
}
