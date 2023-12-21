#pragma once

#include "bvh.h"
#include "ray.h"

class Scene
{
public:
	Scene(BvhBLAS& bvh);

	bool intersect(Ray& ray) const;

	Float3 normal(SizeType instanceIndex, SizeType primitiveIndex) const;

private:
	BvhBLAS& m_bvh;
};
