#pragma once

#include "mesh.h"
#include "ray.h"
#include "surf_math.h"
#include "types.h"

#define BVH_ROOT_INDEX	0

struct AABB
{
	Float3 bbMin	= Float3(F32_INF);
	Float3 bbMax	= Float3(F32_NEG_INF);

	void grow(const Float3& point);

	void grow(const AABB& boundingBox);

	F32 area() const;

	F32 intersect(Ray& ray) const;
};

struct BvhBin
{
	U32 count			= 0;
	AABB boundingBox	= AABB();
};

struct BvhNode
{
	U32 leftFirst;
	U32 count;
	AABB boundingBox;

	inline U32 left() const { return leftFirst; }
	inline U32 right() const { return leftFirst + 1; }
	inline U32 first() const { return leftFirst; }
	inline bool isLeaf() const { return count != 0; }
};

class BvhBLAS
{
public:
	BvhBLAS(Mesh mesh);

	~BvhBLAS();

	BvhBLAS(const BvhBLAS& other) noexcept;
	BvhBLAS& operator=(const BvhBLAS& other) noexcept;

	bool intersect(Ray& ray) const;

	void build();

	void refit();

	inline const Mesh& mesh() const;

private:
	F32 calculateNodeCost(const BvhNode& node) const;

	F32 findSplitPlane(const BvhNode& node, F32& cost, U32& axis) const;

	U32 partitionNode(const BvhNode& node, F32 splitPosition, U32 axis) const;

	void updateNodeBounds(SizeType nodeIndex);

	void subdivide(SizeType nodeIndex);

private:
	Mesh m_mesh;
	SizeType m_triCount;
	U32* m_indices;
	U32 m_nodesUsed;
	BvhNode* m_nodePool;
};

const Mesh& BvhBLAS::mesh() const
{
	return m_mesh;
}
