#pragma once

#include "bvh.h"
#include "mesh.h"
#include "ray.h"
#include "surf_math.h"

class Scene
{
public:
	Scene(BvhBLAS* bvh);

	bool intersect(Ray& ray) const;

	const Mesh* hitMesh(SizeType instanceIndex) const;

private:
	BvhBLAS* m_bvh;
};
