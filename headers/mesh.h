#pragma once

#include <vector>

#include "ray.h"
#include "surf_math.h"
#include "types.h"

struct Triangle
{
	Float3 v0;
	Float3 v1;
	Float3 v2;
	Float3 centroid;

	Triangle(Float3 v1, Float3 v0, Float3 v2);

	bool intersect(Ray& ray) const;

	Float3 normal() const;
};

class Mesh
{
public:
	Mesh();

	Float3 normal(SizeType primitiveIndex) const;

public:
	std::vector<Triangle> m_triangles;
};
