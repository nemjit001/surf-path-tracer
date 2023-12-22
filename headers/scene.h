#pragma once

#include <vector>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

class Scene
{
public:
	Scene(std::vector<Instance> instances);

	bool intersect(Ray& ray) const;

	const Instance& hitInstance(SizeType instanceIndex) const;

private:
	std::vector<Instance> m_instances;
};
