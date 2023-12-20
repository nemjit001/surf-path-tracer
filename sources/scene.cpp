#include "scene.h"

#include "mesh.h"
#include "ray.h"

Scene::Scene()
	:
	m_testMesh()
{
	//
}

bool Scene::intersect(Ray& ray) const
{
	// TODO: intersect scene TLAS

	bool hit = false;
	for (SizeType primIdx = 0; primIdx < m_testMesh.m_triangles.size(); primIdx++)
	{
		auto const& triangle = m_testMesh.m_triangles[primIdx];
		if (triangle.intersect(ray))
		{
			ray.metadata.primitiveIndex = static_cast<U32>(primIdx);
			hit = true;
		}
	}

	return hit;
}

Float3 Scene::normal(SizeType instanceIndex, SizeType primitiveIndex) const
{
	return m_testMesh.normal(primitiveIndex);
}
