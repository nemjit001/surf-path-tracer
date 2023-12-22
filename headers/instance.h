#pragma once

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

class Instance
{
public:
	Instance(BvhBLAS* blas, Mat4 transform);

	bool intersect(Ray& ray) const;

	inline const Mat4& transform() const { return m_transform; }

	inline void setTransform(const Mat4& transform);

public:
	BvhBLAS* bvh;

private:
	Mat4 m_transform;
	Mat4 m_invTransform;
};

void Instance::setTransform(const Mat4& transform)
{
	m_invTransform = glm::inverse(transform);
	m_transform = transform;
}
