#include "scene.h"

#include <cassert>
#include <map>
#include <set>
#include <vector>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

Scene::Scene(SceneBackground background, std::vector<Instance> instances)
	:
	m_background(background),
	m_sceneTlas(instances)
{
	//
}

RgbColor Scene::sampleBackground(const Ray& ray) const
{
	switch (m_background.type)
	{
	case BackgroundType::SolidColor:
		return m_background.color;
	case BackgroundType::ColorGradient:
		{
			F32 alpha = 0.5f * (1.0f + ray.direction.y);
			return alpha * m_background.gradient.colorB + (1.0f - alpha) * m_background.gradient.colorA;
		}
	default:
		break;
	}

	return RgbColor(0.0f);	// Default to black
}

void Scene::update(F32 deltaTime)
{
	m_sceneTlas.refit();
}

const GPUBatchInfo GPUBatcher::createBatchInfo(const std::vector<Instance>& instances)
{
	GPUBatchInfo batchInfo = {};

	std::map<const Mesh*, SizeType> sceneMeshes;
	std::map<const BvhBLAS*, SizeType> sceneBVHIndices;
	std::map<const BvhBLAS*, SizeType> sceneBVHNodes;
	std::set<const Material*> sceneMaterials;

	for (auto const& instance : instances)
	{
		const Mesh* mesh = instance.bvh->mesh();
		assert(mesh->triangles.size() == mesh->triExtensions.size());
		sceneMeshes.insert(std::make_pair(mesh, mesh->triangles.size()));
		sceneBVHIndices.insert(std::make_pair(instance.bvh, mesh->triangles.size()));
		sceneBVHNodes.insert(std::make_pair(instance.bvh, static_cast<SizeType>(instance.bvh->nodesUsed())));
		sceneMaterials.insert(instance.material);
	}

	for (auto const& [ mesh, size ] : sceneMeshes)
	{
		batchInfo.triBuffer.insert(
			batchInfo.triBuffer.end(),
			mesh->triangles.begin(),
			mesh->triangles.end()
		);

		batchInfo.triExtBuffer.insert(
			batchInfo.triExtBuffer.end(),
			mesh->triExtensions.begin(),
			mesh->triExtensions.end()
		);
	}

	for (auto const& [ bvh, size ] : sceneBVHIndices)
	{
		batchInfo.BLASIndices.insert(
			batchInfo.BLASIndices.end(),
			bvh->indices(),
			bvh->indices() + size
		);
	}

	for (auto const& [ bvh, size ] : sceneBVHNodes)
	{
		batchInfo.BLASNodes.insert(
			batchInfo.BLASNodes.end(),
			bvh->nodePool(),
			bvh->nodePool() + size
		);
	}

	for (auto const& material : sceneMaterials)
	{
		batchInfo.materials.push_back(*material);
	}

	for (auto const& instance : instances)
	{
		GPUInstance gpuInstance = instance.toGPUInstance();
		for (auto const& [ mesh, size ] : sceneMeshes)
		{
			if (mesh == instance.bvh->mesh()) break;
			gpuInstance.triOffset += size;
		}

		for (auto const& [ bvh, size ] : sceneBVHIndices)
		{
			if (bvh == instance.bvh) break;
			gpuInstance.bvhIdxOffset += size;
		}

		for (auto const& [ bvh, size ] : sceneBVHNodes)
		{
			if (bvh == instance.bvh) break;
			gpuInstance.bvhNodeOffset += size;
		}

		for (auto const& material : sceneMaterials)
		{
			if (material == instance.material) break;
			gpuInstance.materialOffset++;
		}

		batchInfo.gpuInstances.push_back(gpuInstance);
	}

	return batchInfo;
}

GPUScene::GPUScene(RenderContext* renderContext, SceneBackground background, std::vector<Instance> instances)
	:
	Scene(background, instances),
	batchInfo(GPUBatcher::createBatchInfo(instances)),
	globalTriBuffer(
		renderContext->allocator, batchInfo.triBuffer.size() * sizeof(Triangle),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	globalTriExtBuffer(
		renderContext->allocator, batchInfo.triExtBuffer.size() * sizeof(TriExtension),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	BLASGlobalIndexBuffer(
		renderContext->allocator, batchInfo.BLASIndices.size() * sizeof(U32),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	BLASGlobalNodeBuffer(
		renderContext->allocator, batchInfo.BLASNodes.size() * sizeof(BvhNode),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	materialBuffer(
		renderContext->allocator, batchInfo.materials.size() * sizeof(Material),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	instanceBuffer(
		renderContext->allocator, batchInfo.gpuInstances.size() * sizeof(GPUInstance),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	TLASIndexBuffer(
		renderContext->allocator, instances.size() * sizeof(U32),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	TLASNodeBuffer(
		renderContext->allocator, m_sceneTlas.nodesUsed() * sizeof(BvhNode),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	)
{
	assert(renderContext != nullptr);

	// TODO: upload GPU data using staging buffer
}
