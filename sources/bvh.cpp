#include "bvh.h"

#include "mesh.h"
#include "ray.h"
#include "surf.h"
#include "types.h"

#define TRAVERSAL_STACK_SIZE	64
#define BIN_COUNT				8
#define PLANE_COUNT				BIN_COUNT - 1

void AABB::grow(const Float3& point)
{
	bbMin = min(bbMin, point);
	bbMax = max(bbMax, point);
}

void AABB::grow(const AABB& boundingBox)
{
	bbMin = min(this->bbMin, boundingBox.bbMin);
	bbMax = max(this->bbMax, boundingBox.bbMax);
}

F32 AABB::area() const
{
	Float3 extent = bbMax - bbMin;
	return extent.x * extent.y + extent.y * extent.z + extent.z * extent.x;
}

F32 AABB::intersect(Ray& ray) const
{
	F32 txNear = (bbMin.x - ray.origin.x) / ray.direction.x;
	F32 txFar = (bbMax.x - ray.origin.x) / ray.direction.x;
	F32 tmin = min(txNear, txFar);
	F32 tmax = max(txNear, txFar);

	F32 tyNear = (bbMin.y - ray.origin.y) / ray.direction.y;
	F32 tyFar = (bbMax.y - ray.origin.y) / ray.direction.y;
	tmin = min(tmin, min(tyNear, tyFar));
	tmax = max(tmax, max(tyNear, tyFar));

	F32 tzNear = (bbMin.z - ray.origin.z) / ray.direction.z;
	F32 tzFar = (bbMax.z - ray.origin.z) / ray.direction.z;
	tmin = min(tmin, min(tzNear, tzFar));
	tmax = max(tmax, max(tzNear, tzFar));

	if (tmax >= tmin && tmin < ray.depth && tmax > 0.0f)
	{
		return tmin;
	}

	return F32_FAR_AWAY;
}

BvhBLAS::BvhBLAS(Mesh mesh)
	:
	m_mesh(mesh),
	m_triCount(mesh.triangles.size()),
	m_indices(new U32[m_triCount]{}),
	m_nodesUsed(2),
	m_nodePool(static_cast<BvhNode*>(MALLOC64(2 * m_triCount * sizeof(BvhNode))))
{
	assert(m_nodePool != nullptr && m_indices != nullptr);
	memset(m_nodePool, 0, 2 * m_triCount * sizeof(BvhNode));

	// Fill out indices array
	for (SizeType idx = 0; idx < m_triCount; idx++)
		m_indices[idx] = static_cast<U32>(idx);
}

BvhBLAS::~BvhBLAS()
{
	delete[] m_indices;
	FREE64(m_nodePool);
}

BvhBLAS::BvhBLAS(const BvhBLAS& other) noexcept
	:
	m_mesh(other.m_mesh),
	m_triCount(other.m_triCount),
	m_indices(nullptr),
	m_nodesUsed(other.m_nodesUsed),
	m_nodePool(nullptr)
{
	m_indices = new U32[m_mesh.triangles.size()];
	m_nodePool = static_cast<BvhNode*>(MALLOC64(2 * m_triCount * sizeof(BvhNode)));

	assert(m_nodePool != nullptr && m_indices != nullptr);

	memcpy(m_indices, other.m_indices, sizeof(U32) * m_triCount);
	memcpy(m_nodePool, other.m_nodePool, 2 * m_triCount * sizeof(BvhNode));
}

BvhBLAS& BvhBLAS::operator=(const BvhBLAS& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	this->m_mesh = other.m_mesh;
	this->m_triCount = other.m_triCount;
	this->m_indices = new U32[m_triCount]{};
	memcpy(this->m_indices, other.m_indices, sizeof(U32) * m_triCount);

	this->m_nodesUsed = other.m_nodesUsed;
	this->m_nodePool = static_cast<BvhNode*>(MALLOC64(2 * m_triCount * sizeof(BvhNode)));
	memcpy(m_nodePool, other.m_nodePool, 2 * m_triCount * sizeof(BvhNode));

	return *this;
}

bool BvhBLAS::intersect(Ray& ray) const
{
	BvhNode* node = &m_nodePool[BVH_ROOT_INDEX];
	BvhNode* stack[TRAVERSAL_STACK_SIZE]{};
	SizeType stackPtr = 0;

	bool intersected = false;
	while(true)
	{
		if (node->isLeaf())
		{
			for (U32 i = 0; i < node->count; i++)
			{
				U32 primitiveIndex = m_indices[node->first() + i];
				const Triangle& tri = m_mesh.triangles[primitiveIndex];

				if (tri.intersect(ray))
				{
					intersected = true;
					ray.metadata.primitiveIndex = primitiveIndex;
				}
			}

			if (stackPtr == 0)
				break;

			node = stack[stackPtr - 1];
			stackPtr--;
			continue;
		}

		BvhNode* childNear = &m_nodePool[node->left()];
		BvhNode* childFar = &m_nodePool[node->right()];

		F32 distNear = childNear->boundingBox.intersect(ray);
		F32 distFar = childFar->boundingBox.intersect(ray);

		if (distNear > distFar) {
			swap(distNear, distFar);
			swap(childNear, childFar);
		}

		if (distNear == F32_FAR_AWAY)
		{
			if (stackPtr == 0)
				break;

			node = stack[stackPtr - 1];
			stackPtr--;
		}
		else
		{
			node = childNear;
			if (distFar != F32_FAR_AWAY)
			{
				stack[stackPtr] = childFar;
				stackPtr++;
			}
		}
	}

	return intersected;
}

void BvhBLAS::build()
{
	// Reset nodes used
	m_nodesUsed = 2;

	BvhNode& rootNode = m_nodePool[BVH_ROOT_INDEX];
	rootNode.leftFirst = 0;
	rootNode.count = static_cast<U32>(m_triCount);

	updateNodeBounds(BVH_ROOT_INDEX);
	subdivide(BVH_ROOT_INDEX);
}

void BvhBLAS::refit()
{
	// refit bvh
}

F32 BvhBLAS::calculateNodeCost(const BvhNode& node) const
{
	return static_cast<F32>(node.count) * node.boundingBox.area();
}

F32 BvhBLAS::findSplitPlane(const BvhNode& node, F32& cost, U32& axis) const
{
	F32 bestCost = F32_INF;
	F32 bestSplit = 0.0f;
	U32 bestAxis = 0;

	for (U32 axis = 0; axis < 3; axis++)
	{
		// Calculate centroid based bin extent
		F32 boundsMin = F32_MAX;
		F32 boundsMax = F32_MIN;
		for (SizeType i = 0; i < node.count; i++)
		{
			SizeType idx = node.first() + i;
			const Triangle& tri = m_mesh.triangles[m_indices[idx]];

			boundsMin = min(boundsMin, tri.centroid[axis]);
			boundsMax = max(boundsMax, tri.centroid[axis]);
		}

		if (boundsMin == boundsMax)
		{
			continue;
		}

		// Populate BVH bins
		const F32 binScale = static_cast<F32>(BIN_COUNT) / (boundsMax - boundsMin);
		BvhBin bins[BIN_COUNT];

		for (SizeType i = 0; i < node.count; i++)
		{
			SizeType idx = node.first() + i;
			const Triangle& tri = m_mesh.triangles[m_indices[idx]];

			SizeType section = static_cast<SizeType>((tri.centroid[axis] - boundsMin) * binScale);
			SizeType binIndex = min(BIN_COUNT - 1, section);
			
			BvhBin& bin = bins[binIndex];
			bin.count++;
			bin.boundingBox.grow(tri.v0);
			bin.boundingBox.grow(tri.v1);
			bin.boundingBox.grow(tri.v2);
		}

		// Calculate areas & primitive counts for bins
		F32 leftArea[PLANE_COUNT]{}, rightArea[PLANE_COUNT]{};
		U32 leftCount[PLANE_COUNT]{}, rightCount[PLANE_COUNT]{};
		AABB leftBox = AABB(), rightBox = AABB();
		U32 leftSum = 0, rightSum = 0;

		for (SizeType planeIdx = 0; planeIdx < PLANE_COUNT; planeIdx++)
		{
			leftSum += bins[planeIdx].count;
			leftCount[planeIdx] = leftSum;
			leftBox.grow(bins[planeIdx].boundingBox);
			leftArea[planeIdx] = leftBox.area();

			const SizeType RIGHT_BIN = BIN_COUNT - 1 - planeIdx;
			const SizeType RIGHT_PLANE = RIGHT_BIN - 1;
			rightSum += bins[RIGHT_BIN].count;
			rightCount[RIGHT_PLANE] = rightSum;
			rightBox.grow(bins[RIGHT_BIN].boundingBox);
			rightArea[RIGHT_PLANE] = rightBox.area();
		}

		// Calculate best split position
		F32 binExtent = (boundsMax - boundsMin) / static_cast<F32>(BIN_COUNT);
		for (SizeType planeIdx = 0; planeIdx < PLANE_COUNT; planeIdx++)
		{
			F32 planeCost = leftCount[planeIdx] * leftArea[planeIdx] + rightCount[planeIdx] * rightArea[planeIdx];

			if (planeCost < bestCost)
			{
				bestCost = planeCost;
				bestSplit = boundsMin + binExtent * (planeIdx + 1);
				bestAxis = axis;
			}
		}
	}

	cost = bestCost;
	axis = bestAxis;
	return bestSplit;
}

U32 BvhBLAS::partitionNode(const BvhNode& node, F32 splitPosition, U32 axis) const
{
	assert(axis >= 0 && axis < 3);

	I32 pivot = node.first();
	I32 last = pivot + (node.count - 1);

	while (pivot <= last)
	{
		const Triangle& tri = m_mesh.triangles[m_indices[pivot]];
		if (tri.centroid[axis] < splitPosition)
		{
			pivot++;
		}
		else
		{
			swap(m_indices[pivot], m_indices[last]);
			last--;
		}
	}

	return pivot;
}

void BvhBLAS::updateNodeBounds(SizeType nodeIndex)
{
	assert(nodeIndex < 2 * m_triCount);
	BvhNode& node = m_nodePool[nodeIndex];

	for (SizeType i = 0; i < node.count; i++)
	{
		SizeType idx = node.first() + i;
		const Triangle& tri = m_mesh.triangles[m_indices[idx]];
		node.boundingBox.grow(tri.v0);
		node.boundingBox.grow(tri.v1);
		node.boundingBox.grow(tri.v2);
	}
}

void BvhBLAS::subdivide(SizeType nodeIndex)
{
	assert(nodeIndex < 2 * m_triCount);
	BvhNode& node = m_nodePool[nodeIndex];

	F32 cost = F32_INF;
	U32 axis = 0;
	F32 splitPosition = findSplitPlane(node, cost, axis);
	F32 parentCost = calculateNodeCost(node);

	if (cost >= parentCost)
	{
		return;
	}

	U32 pivotPosition = partitionNode(node, splitPosition, axis);
	U32 leftCount = pivotPosition - node.first();

	if (leftCount == 0 || leftCount == node.count)
	{
		return;
	}

	U32 leftIndex = m_nodesUsed;
	U32 rightIndex = m_nodesUsed + 1;
	m_nodesUsed += 2;

	// Update left node
	BvhNode& left = m_nodePool[leftIndex];
	left.leftFirst = node.first();
	left.count = leftCount;
	left.boundingBox = AABB();

	// Update right node
	BvhNode& right = m_nodePool[rightIndex];
	right.leftFirst = pivotPosition;
	right.count = node.count - leftCount;
	right.boundingBox = AABB();

	// Update parent node
	node.leftFirst = leftIndex;
	node.count = 0;

	updateNodeBounds(leftIndex);
	updateNodeBounds(rightIndex);
	subdivide(leftIndex);
	subdivide(rightIndex);
}
