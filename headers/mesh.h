#pragma once

#include <string>
#include <vector>

#include "ray.h"
#include "surf_math.h"
#include "types.h"

struct Triangle
{
	Float3 v0;
	Float3 v1;
	Float3 v2;
	Float3 centroid;

	Triangle(Float3 v1, Float3 v0, Float3 v2);

	bool intersect(Ray& ray) const;

	Float3 normal() const;
};

struct TriExtension
{
	Float3 n0, n1, n2;
	Float2 uv0, uv1, uv2;
};

class Mesh
{
public:
	Mesh(const std::string& path);

	inline Float3 normal(SizeType primitiveIndex) const;

	inline Float3 normal(SizeType primitiveIndex, const Float2& barycentric) const;

	inline Float2 textureCoordinate(SizeType primitiveIndex, const Float2& barycentric) const;

public:
	std::vector<Triangle> triangles;

private:
	std::vector<TriExtension> m_triExtensions;
};

Float3 Mesh::normal(SizeType primitiveIndex) const
{
	assert(primitiveIndex < triangles.size());
	return triangles[primitiveIndex].normal();
}

Float3 Mesh::normal(SizeType primitiveIndex, const Float2& barycentric) const
{
	assert(primitiveIndex < triangles.size());
	const TriExtension& ext = m_triExtensions[primitiveIndex];
	return barycentric.u * ext.n0 + barycentric.v * ext.n2 + (1.0f - barycentric.u - barycentric.v) * ext.n1;
}

Float2 Mesh::textureCoordinate(SizeType primitiveIndex, const Float2& barycentric) const
{
	assert(primitiveIndex < triangles.size());
	const TriExtension& ext = m_triExtensions[primitiveIndex];
	return barycentric.u * ext.uv0 + barycentric.v * ext.uv2 + (1.0f - barycentric.u - barycentric.v) * ext.uv1;
}
