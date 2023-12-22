#include "scene.h"

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

Scene::Scene(Instance instance)
	:
	m_instance(instance)
{
	//
}

bool Scene::intersect(Ray& ray) const
{
	// TODO: intersect scene TLAS
	return m_instance.intersect(ray);
}

const Instance& Scene::hitInstance(SizeType instanceIndex) const
{
	return m_instance;
}
