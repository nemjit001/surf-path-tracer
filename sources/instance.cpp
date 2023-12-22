#include "instance.h"

#include <cassert>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

Instance::Instance(BvhBLAS* blas, Mat4 transform)
	:
	bvh(blas),
	m_transform(transform),
	m_invTransform(glm::inverse(transform))
{
	//
}

bool Instance::intersect(Ray& ray) const
{
	assert(bvh != nullptr);
	Ray oldRay = ray;

	glm::vec4 tPos = m_invTransform * static_cast<glm::vec4>(Float4(ray.origin, 1.0f));
	glm::vec4 tDir = m_invTransform * static_cast<glm::vec4>(Float4(ray.direction, 0.0f));
	ray.origin = Float3(tPos.x, tPos.y, tPos.z) / tPos.w;
	ray.direction = Float3(tDir.x, tDir.y, tDir.z);

	bool intersected = bvh->intersect(ray);
	ray.origin = oldRay.origin;
	ray.direction = oldRay.direction;

	return intersected;
}
