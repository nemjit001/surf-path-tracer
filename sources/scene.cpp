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
#if 0
	bool intersected = false;
	const auto& tris = m_bvh->mesh()->triangles;
	SizeType triCount = tris.size();

	for (SizeType i = 0; i < triCount; i++)
	{
		const Triangle& tri = tris[i];
		if (tri.intersect(ray))
		{
			ray.metadata.primitiveIndex = static_cast<U32>(i);
			intersected = true;
		}
	}

	return intersected;
#else
	return m_bvh->intersect(ray);
#endif
}

Float3 Scene::normal(SizeType instanceIndex, SizeType primitiveIndex) const
{
	return m_bvh->mesh()->normal(primitiveIndex);
}
