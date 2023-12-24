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
	ray.metadata.hitCoordinates = Float2(u, v);
	return true;
}

Float3 Triangle::normal() const
{
	return (v1 - v0).cross(v2 - v0).normalize();
}

Mesh::Mesh(const std::string& path)
	:
	triangles(),
	m_triExtensions()
{
	tinyobj::ObjReaderConfig config;
	config.triangulate = true;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(path, config))
	{
		std::cerr << "Error reading file: " << path << '\n';
		if (!reader.Error().empty())
		{
			std::cerr << reader.Error() << '\n';
		}

		FATAL_ERROR("Failed to read OBJ file");
	}

	if (!reader.Warning().empty())
	{
		std::cerr << reader.Warning() << '\n';
	}

	const tinyobj::attrib_t& attributes = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();

	for (const auto& shape : shapes)
	{
		for (SizeType i = 0; i < shape.mesh.indices.size(); i+= 3)
		{
			tinyobj::index_t index0 = shape.mesh.indices[i + 0];
			tinyobj::index_t index1 = shape.mesh.indices[i + 1];
			tinyobj::index_t index2 = shape.mesh.indices[i + 2];

			triangles.push_back(Triangle(
				Float3(
					attributes.vertices[3 * index0.vertex_index + 0],
					attributes.vertices[3 * index0.vertex_index + 1],
					attributes.vertices[3 * index0.vertex_index + 2]
				),
				Float3(
					attributes.vertices[3 * index1.vertex_index + 0],
					attributes.vertices[3 * index1.vertex_index + 1],
					attributes.vertices[3 * index1.vertex_index + 2]
				),
				Float3(
					attributes.vertices[3 * index2.vertex_index + 0],
					attributes.vertices[3 * index2.vertex_index + 1],
					attributes.vertices[3 * index2.vertex_index + 2]
				)
			));

			m_triExtensions.push_back(TriExtension{
				Float3(
					attributes.normals[3 * index0.normal_index + 0],
					attributes.normals[3 * index0.normal_index + 1],
					attributes.normals[3 * index0.normal_index + 2]
				),
				Float3(
					attributes.normals[3 * index1.normal_index + 0],
					attributes.normals[3 * index1.normal_index + 1],
					attributes.normals[3 * index1.normal_index + 2]
				),
				Float3(
					attributes.normals[3 * index2.normal_index + 0],
					attributes.normals[3 * index2.normal_index + 1],
					attributes.normals[3 * index2.normal_index + 2]
				),
				Float2(
					attributes.texcoords[2 * index0.texcoord_index + 0],
					attributes.texcoords[2 * index0.texcoord_index + 1]
				),
				Float2(
					attributes.texcoords[2 * index1.texcoord_index + 0],
					attributes.texcoords[2 * index1.texcoord_index + 1]
				),
				Float2(
					attributes.texcoords[2 * index2.texcoord_index + 0],
					attributes.texcoords[2 * index2.texcoord_index + 1]
				)
			});
		}
	}
}
