#include "scene.h"

#include <cassert>
#include <vector>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

Scene::Scene(std::vector<Instance> instances)
	:
	m_instances(instances)
{
	//
}

bool Scene::intersect(Ray& ray) const
{
	bool intersected = false;
	for (SizeType i = 0; i < m_instances.size(); i++)
	{
		const Instance& instance = m_instances[i];
		if (instance.intersect(ray))
		{
			ray.metadata.instanceIndex = static_cast<U32>(i);
			intersected = true;
		}
	}

	return intersected;
}

const Instance& Scene::hitInstance(SizeType instanceIndex) const
{
	assert(instanceIndex < m_instances.size());
	return m_instances[instanceIndex];
}
