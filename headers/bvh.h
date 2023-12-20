#pragma once

#include <vector>

#include "mesh.h"
#include "ray.h"
#include "types.h"

#define ROOT_INDEX	0

struct BvhNode
{
	U32 left;
	U32 right;
	U32 first;
	U32 count;
};

class BvhBLAS
{
public:
	BvhBLAS(Mesh mesh);

	~BvhBLAS();

	bool intersect(Ray& ray);

	void build();

	void refit();

	inline const Mesh& mesh() const;

private:
	Mesh m_mesh;
	U32* m_indices;
	U32 m_nodesUsed;
	BvhNode* m_nodePool;
};

const Mesh& BvhBLAS::mesh() const
{
	return m_mesh;
}
