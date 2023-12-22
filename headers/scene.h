#pragma once

#include "instance.h"
#include "ray.h"
#include "surf_math.h"

class Scene
{
public:
	Scene(Instance instance);

	bool intersect(Ray& ray) const;

	const Instance& hitInstance(SizeType instanceIndex) const;

private:
	Instance m_instance;
};
