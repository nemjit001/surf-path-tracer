#include "scene.h"

#include <cassert>
#include <vector>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

Scene::Scene(std::vector<Instance> instances)
	:
	m_sceneTlas(instances)
{
	//
}

bool Scene::intersect(Ray& ray) const
{
	return m_sceneTlas.intersect(ray);
}

const Instance& Scene::hitInstance(SizeType instanceIndex) const
{
	return m_sceneTlas.instance(instanceIndex);
}
