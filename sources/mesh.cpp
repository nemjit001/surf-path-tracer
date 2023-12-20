#include "mesh.h"

#include <vector>

#include "ray.h"
#include "surf_math.h"
#include "types.h"

Triangle::Triangle(Float3 v1, Float3 v0, Float3 v2)
	:
	v0(v0),
	v1(v1),
	v2(v2),
	centroid(0.0f)
{
	centroid = (v0 + v1 + v2) * 0.333f;
}

bool Triangle::intersect(Ray& ray) const
{
	Float3 e1 = v1 - v0;
	Float3 e2 = v2 - v0;

	Float3 h = glm::cross(ray.direction, e2);
	F32 a = glm::dot(e1, h);

	if (fabsf(a) < F32_EPSILON)
	{
		return false;
	}

	F32 f = 1.0f / a;
	Float3 s = ray.origin - v0;
	F32 u = f * glm::dot(s, h);

	if (0.0f > u || u > 1.0f)
	{
		return false;
	}

	Float3 q = glm::cross(s, e1);
	F32 v = f * glm::dot(ray.direction, q);

	if (0.0f > v || (u + v) > 1.0f)
	{
		return false;
	}

	F32 depth = f * glm::dot(e2, q);
	if (!depthInBounds(depth, ray.depth))
	{
		return false;
	}

	ray.depth = depth;
	ray.metadata.u = u;
	ray.metadata.v = v;
	return true;
}

Float3 Triangle::normal() const
{
	return glm::normalize(glm::cross(v1 - v0, v2 - v0));
}

Mesh::Mesh()
	:
	m_triangles()
{
	m_triangles.reserve(100);

	for (SizeType i = 0; i < 100; i++)
	{
		Float3 v0(randomRange(10.0f) - 5.0f, randomRange(10.0f) - 5.0f, randomRange(10.0f) - 5.0f);
		Float3 v1(v0.x + randomF32(), v0.y + randomF32(), v0.z + randomF32());
		Float3 v2(v0.x + randomF32(), v0.y + randomF32(), v0.z + randomF32());

		m_triangles.push_back(Triangle(v0, v1, v2));
	}
}

Float3 Mesh::normal(SizeType primitiveIndex) const
{
	assert(primitiveIndex < m_triangles.size());
	return m_triangles[primitiveIndex].normal();
}
