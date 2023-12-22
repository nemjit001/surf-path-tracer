#include "instance.h"

#include <cassert>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

bool Instance::intersect(Ray& ray) const
{
	assert(bvh != nullptr);
	return bvh->intersect(ray);
}
