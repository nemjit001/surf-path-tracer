#include "mesh.h"

#include <cassert>
#include <iostream>
#include <string>
#include <tiny_obj_loader.h>
#include <vector>

#include "ray.h"
#include "surf_math.h"
#include "types.h"

Triangle::Triangle(Float3 v1, Float3 v0, Float3 v2)
	:
	v0(v0),
	v1(v1),
	v2(v2),
	centroid(0.0f)
{
	centroid = (v0 + v1 + v2) * 0.333f;
}

bool Triangle::intersect(Ray& ray) const
{
	Float3 e1 = v1 - v0;
	Float3 e2 = v2 - v0;

	Float3 h = ray.direction.cross(e2);
	F32 a = e1.dot(h);

	if (fabsf(a) < F32_EPSILON)
	{
		return false;
	}

	F32 f = 1.0f / a;
	Float3 s = ray.origin - v0;
	F32 u = f * s.dot(h);

	if (0.0f > u || u > 1.0f)
	{
		return false;
	}

	Float3 q = s.cross(e1);
	F32 v = f * ray.direction.dot(q);

	if (0.0f > v || (u + v) > 1.0f)
	{
		return false;
	}

	F32 depth = f * e2.dot(q);
	if (!depthInBounds(depth, ray.depth))
	{
		return false;
	}

	ray.depth = depth;
	ray.metadata.u = u;
	ray.metadata.v = v;
	return true;
}

Float3 Triangle::normal() const
{
	return (v1 - v0).cross(v2 - v0).normalize();
}

Mesh::Mesh(const std::string& path)
	:
	triangles()
{
	tinyobj::ObjReaderConfig config;
	config.triangulate = true;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(path, config))
	{
		std::cerr << "Failed to read OBJ file: " << path << '\n';
		if (!reader.Error().empty())
		{
			std::cerr << reader.Error() << '\n';
		}

		abort();
	}

	if (!reader.Warning().empty())
	{
		std::cerr << reader.Warning() << '\n';
	}

	const tinyobj::attrib_t& attributes = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();

	std::vector<Float3> vertices;
	std::vector<U32> indices;
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			SizeType vertexIndex = 3 * index.vertex_index;
			Float3 position(
				attributes.vertices[vertexIndex + 0],
				attributes.vertices[vertexIndex + 1],
				attributes.vertices[vertexIndex + 2]
			);

			vertices.push_back(position);
			indices.push_back(static_cast<U32>(indices.size()));
		}
	}

	triangles.reserve(indices.size() / 3);
	for (SizeType i = 0; i < indices.size(); i += 3)
	{
		triangles.push_back(Triangle(
			vertices[indices[i]],
			vertices[indices[i + 1]],
			vertices[indices[i + 2]]
		));
	}
}

Float3 Mesh::normal(SizeType primitiveIndex) const
{
	assert(primitiveIndex < triangles.size());
	return triangles[primitiveIndex].normal();
}
