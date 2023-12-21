#include "scene.h"

#include "mesh.h"
#include "ray.h"

Scene::Scene(BvhBLAS* bvh)
	:
	m_bvh(bvh)
{
	m_bvh->build();
}

bool Scene::intersect(Ray& ray) const
{
	// TODO: intersect scene TLAS
	return m_bvh->intersect(ray);
}

Float3 Scene::normal(SizeType instanceIndex, SizeType primitiveIndex) const
{
	return m_bvh->mesh()->normal(primitiveIndex);
}
