#include "bvh.h"

#include <cassert>
#include <cstring>

#include "material.h"
#include "mesh.h"
#include "ray.h"
#include "surf.h"
#include "surf_math.h"
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

Float3 AABB::center() const
{
	return 0.5f * (bbMax - bbMin);
}

F32 AABB::intersect(Ray& ray) const
{
	// calculate direction reciprocal once
	Float3 rDir = Float3(1.0f) / ray.direction;

	F32 txNear = (bbMin.x - ray.origin.x) * rDir.x;
	F32 txFar = (bbMax.x - ray.origin.x) * rDir.x;
	F32 tmin = min(txNear, txFar);
	F32 tmax = max(txNear, txFar);

	F32 tyNear = (bbMin.y - ray.origin.y) * rDir.y;
	F32 tyFar = (bbMax.y - ray.origin.y) * rDir.y;
	tmin = max(tmin, min(tyNear, tyFar));
	tmax = min(tmax, max(tyNear, tyFar));

	F32 tzNear = (bbMin.z - ray.origin.z) * rDir.z;
	F32 tzFar = (bbMax.z - ray.origin.z) * rDir.z;
	tmin = max(tmin, min(tzNear, tzFar));
	tmax = min(tmax, max(tzNear, tzFar));

	if (tmax >= tmin && tmin < ray.depth && tmax > 0.0f)
	{
		return tmin;
	}

	return F32_FAR_AWAY;
}

BvhBLAS::BvhBLAS(Mesh* mesh)
	:
	m_mesh(mesh),
	m_triCount(mesh->triangles.size()),
	m_indices(new U32[m_triCount]{}),
	m_nodesUsed(2),
	m_nodePool(static_cast<BvhNode*>(MALLOC64(2 * m_triCount * sizeof(BvhNode))))
{
	assert(m_nodePool != nullptr && m_indices != nullptr);
	memset(m_nodePool, 0, 2 * m_triCount * sizeof(BvhNode));

	// Fill out indices array
	for (SizeType idx = 0; idx < m_triCount; idx++)
		m_indices[idx] = static_cast<U32>(idx);

	// Build BVH based on loaded mesh
	build();
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
	m_indices = new U32[m_triCount];
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
				const Triangle& tri = m_mesh->triangles[primitiveIndex];

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

bool BvhBLAS::intersectAny(Ray& ray) const
{
	BvhNode* node = &m_nodePool[BVH_ROOT_INDEX];
	BvhNode* stack[TRAVERSAL_STACK_SIZE]{};
	SizeType stackPtr = 0;

	while (true)
	{
		if (node->isLeaf())
		{
			for (U32 i = 0; i < node->count; i++)
			{
				U32 primitiveIndex = m_indices[node->first() + i];
				const Triangle& tri = m_mesh->triangles[primitiveIndex];

				if (tri.intersect(ray))
				{
					return true;
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

	return false;
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
	FATAL_ERROR("Refitting not yet implemented!\n");
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
			const Triangle& tri = m_mesh->triangles[m_indices[idx]];

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
			const Triangle& tri = m_mesh->triangles[m_indices[idx]];

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
		const Triangle& tri = m_mesh->triangles[m_indices[pivot]];
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
		const Triangle& tri = m_mesh->triangles[m_indices[idx]];
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

Instance::Instance(BvhBLAS* blas, Material* material, Mat4 transform)
	:
	bvh(blas),
	material(material),
	bounds(),
	m_transform(transform),
	m_invTransform(1.0f)
{
	assert(bvh != nullptr);
	assert(material != nullptr);

	setTransform(transform);
}

bool Instance::intersect(Ray& ray) const
{
	assert(bvh != nullptr);
	Ray oldRay = ray;

	glm::vec4 tPos = m_invTransform * static_cast<glm::vec4>(Float4(ray.origin, 1.0f));
	glm::vec4 tDir = m_invTransform * static_cast<glm::vec4>(Float4(ray.direction, 0.0f));
	ray.origin = Float3(tPos.x, tPos.y, tPos.z) / tPos.w;
	ray.direction = Float3(tDir.x, tDir.y, tDir.z);

	bool intersected = bvh->intersect(ray);
	ray.origin = oldRay.origin;
	ray.direction = oldRay.direction;

	return intersected;
}

Float3 Instance::normal(U32 primitiveIndex, const Float2& barycentric) const
{
	Float3 normal = bvh->mesh()->normal(primitiveIndex, barycentric);
	glm::vec4 tNormal = m_transform * static_cast<glm::vec4>(Float4(normal, 0.0f));
	tNormal = glm::normalize(tNormal);	// Renormalize to avoid rounding errors

	return Float3(tNormal.x, tNormal.y, tNormal.z);
}

void Instance::setTransform(const Mat4& transform)
{
	const AABB& localBounds = bvh->bounds();
	bounds = AABB();

	Float3 positions[] = {
		Float3(localBounds.bbMax.x, localBounds.bbMax.y, localBounds.bbMax.z),
		Float3(localBounds.bbMin.x, localBounds.bbMax.y, localBounds.bbMax.z),
		Float3(localBounds.bbMax.x, localBounds.bbMin.y, localBounds.bbMax.z),
		Float3(localBounds.bbMin.x, localBounds.bbMin.y, localBounds.bbMax.z),
		Float3(localBounds.bbMax.x, localBounds.bbMax.y, localBounds.bbMin.z),
		Float3(localBounds.bbMin.x, localBounds.bbMax.y, localBounds.bbMin.z),
		Float3(localBounds.bbMax.x, localBounds.bbMin.y, localBounds.bbMin.z),
		Float3(localBounds.bbMin.x, localBounds.bbMin.y, localBounds.bbMin.z),
	};

	m_invTransform = glm::inverse(transform);
	m_transform = transform;

	for (auto const& pos : positions)
	{
		glm::vec4 transformedPos = transform * static_cast<glm::vec4>(Float4(pos, 1.0f));
		bounds.grow(Float3(transformedPos.x, transformedPos.y, transformedPos.z) / transformedPos.w);
	}
}

BvhTLAS::BvhTLAS(std::vector<Instance> instances)
	:
	m_instances(instances),
	m_indices(new U32[m_instances.size()]{}),
	m_nodesUsed(2),
	m_nodePool(static_cast<BvhNode*>(MALLOC64(2 * m_instances.size() * sizeof(BvhNode))))
{
	assert(m_nodePool != nullptr && m_indices != nullptr);
	memset(m_nodePool, 0, 2 * m_instances.size() * sizeof(BvhNode));

	// Fill out indices array
	for (SizeType idx = 0; idx < m_instances.size(); idx++)
		m_indices[idx] = static_cast<U32>(idx);

	// Build BVH based on instances
	build();
}

BvhTLAS::~BvhTLAS()
{
	delete[] m_indices;
	FREE64(m_nodePool);
}

BvhTLAS::BvhTLAS(const BvhTLAS& other) noexcept
	:
	m_instances(other.m_instances),
	m_indices(nullptr),
	m_nodesUsed(other.m_nodesUsed),
	m_nodePool(nullptr)
{
	m_indices = new U32[m_instances.size()];
	m_nodePool = static_cast<BvhNode*>(MALLOC64(2 * m_instances.size() * sizeof(BvhNode)));

	assert(m_nodePool != nullptr && m_indices != nullptr);

	memcpy(m_indices, other.m_indices, sizeof(U32) * m_instances.size());
	memcpy(m_nodePool, other.m_nodePool, 2 * m_instances.size() * sizeof(BvhNode));
}

BvhTLAS& BvhTLAS::operator=(const BvhTLAS& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	this->m_instances = other.m_instances;
	this->m_indices = new U32[m_instances.size()]{};
	memcpy(this->m_indices, other.m_indices, sizeof(U32) * m_instances.size());

	this->m_nodesUsed = other.m_nodesUsed;
	this->m_nodePool = static_cast<BvhNode*>(MALLOC64(2 * m_instances.size() * sizeof(BvhNode)));
	memcpy(m_nodePool, other.m_nodePool, 2 * m_instances.size() * sizeof(BvhNode));

	return *this;
}

bool BvhTLAS::intersect(Ray& ray) const
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
				U32 instanceIndex = m_indices[node->first() + i];
				const Instance& instance = m_instances[instanceIndex];

				if (instance.intersect(ray))
				{
					intersected = true;
					ray.metadata.instanceIndex = instanceIndex;
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

bool BvhTLAS::intersectAny(Ray& ray) const
{
	BvhNode* node = &m_nodePool[BVH_ROOT_INDEX];
	BvhNode* stack[TRAVERSAL_STACK_SIZE]{};
	SizeType stackPtr = 0;

	while(true)
	{
		if (node->isLeaf())
		{
			for (U32 i = 0; i < node->count; i++)
			{
				U32 instanceIndex = m_indices[node->first() + i];
				const Instance& instance = m_instances[instanceIndex];

				if (instance.intersect(ray))
				{
					return true;
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

	return false;
}

void BvhTLAS::build()
{
	// Reset nodes used
	m_nodesUsed = 2;

	BvhNode& rootNode = m_nodePool[BVH_ROOT_INDEX];
	rootNode.leftFirst = 0;
	rootNode.count = static_cast<U32>(m_instances.size());

	updateNodeBounds(BVH_ROOT_INDEX);
	subdivide(BVH_ROOT_INDEX);
}

void BvhTLAS::refit()
{
	FATAL_ERROR("Refitting NYI for TLAS\n");
}

F32 BvhTLAS::calculateNodeCost(const BvhNode& node) const
{
	return static_cast<F32>(node.count) * node.boundingBox.area();
}

F32 BvhTLAS::findSplitPlane(const BvhNode& node, F32& cost, U32& axis) const
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
			const AABB& bounds = m_instances[m_indices[idx]].bounds;

			boundsMin = min(boundsMin, bounds.center()[axis]);
			boundsMax = max(boundsMax, bounds.center()[axis]);
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
			const AABB& bounds = m_instances[m_indices[idx]].bounds;

			SizeType section = static_cast<SizeType>((bounds.center()[axis] - boundsMin) * binScale);
			SizeType binIndex = min(BIN_COUNT - 1, section);
			
			BvhBin& bin = bins[binIndex];
			bin.count++;
			bin.boundingBox.grow(bounds);
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

U32 BvhTLAS::partitionNode(const BvhNode& node, F32 splitPosition, U32 axis) const
{
	assert(axis >= 0 && axis < 3);

	I32 pivot = node.first();
	I32 last = pivot + (node.count - 1);

	while (pivot <= last)
	{
		const AABB& bounds = m_instances[m_indices[pivot]].bounds;
		if (bounds.center()[axis] < splitPosition)
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

void BvhTLAS::updateNodeBounds(SizeType nodeIndex)
{
	assert(nodeIndex < 2 * m_instances.size());
	BvhNode& node = m_nodePool[nodeIndex];

	for (SizeType i = 0; i < node.count; i++)
	{
		SizeType idx = node.first() + i;
		const AABB& bounds = m_instances[m_indices[idx]].bounds;
		node.boundingBox.grow(bounds);
	}
}

void BvhTLAS::subdivide(SizeType nodeIndex)
{
	assert(nodeIndex < 2 * m_instances.size());
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
