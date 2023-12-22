#pragma once

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

struct Instance
{
	BvhBLAS* bvh;

	bool intersect(Ray& ray) const;
};
