#pragma once

#include "mesh.h"
#include "ray.h"

class Scene
{
public:
	Scene();

	bool intersect(Ray& ray) const;

	Float3 normal(SizeType instanceIndex, SizeType primitiveIndex) const;

private:
	Mesh m_testMesh;
};
