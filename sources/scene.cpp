#include "scene.h"

#include <cassert>
#include <map>
#include <set>
#include <vector>
#include <vulkan/vulkan.h>

#include "bvh.h"
#include "camera.h"
#include "ray.h"
#include "render_context.h"
#include "surf_math.h"
#include "vk_layer/buffer.h"
#include "vk_layer/vk_check.h"

Scene::Scene(SceneBackground background, std::vector<Instance> instances)
	:
	m_background(background),
	m_sceneTlas(instances)
{
	// Collect lights in scene
	SizeType idx = 0;
	for (auto const& instance : instances)
	{
		if (instance.material->isLight())
		{
			m_lightIndices.push_back(static_cast<U32>(idx));
		}

		idx++;
	}
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
	Instance& instance = m_sceneTlas.instance(3);
	instance.setTransform(glm::rotate(instance.transform(), 1.0f * deltaTime, static_cast<glm::vec3>(WORLD_UP)));

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
			gpuInstance.triOffset += static_cast<U32>(size);
		}

		for (auto const& [ bvh, size ] : sceneBVHIndices)
		{
			if (bvh == instance.bvh) break;
			gpuInstance.bvhIdxOffset += static_cast<U32>(size);
		}

		for (auto const& [ bvh, size ] : sceneBVHNodes)
		{
			if (bvh == instance.bvh) break;
			gpuInstance.bvhNodeOffset += static_cast<U32>(size);
		}

		for (auto const& material : sceneMaterials)
		{
			if (material == instance.material) break;
			gpuInstance.materialOffset++;
		}

		if (instance.material->isLight())
		{
			batchInfo.lights.push_back(GPULightData{
				static_cast<U32>(batchInfo.gpuInstances.size()),
				static_cast<U32>(instance.bvh->triCount()),
			});
		}

		batchInfo.gpuInstances.push_back(gpuInstance);
	}

	return batchInfo;
}

GPUScene::GPUScene(RenderContext* renderContext, SceneBackground background, std::vector<Instance> instances)
	:
	m_background(background),
	m_sceneTlas(instances),
	m_renderContext(renderContext),
	m_batchInfo(GPUBatcher::createBatchInfo(instances)),
	globalTriBuffer(
		renderContext->allocator, m_batchInfo.triBuffer.size() * sizeof(Triangle),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	globalTriExtBuffer(
		renderContext->allocator, m_batchInfo.triExtBuffer.size() * sizeof(TriExtension),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	BLASGlobalIndexBuffer(
		renderContext->allocator, m_batchInfo.BLASIndices.size() * sizeof(U32),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	BLASGlobalNodeBuffer(
		renderContext->allocator, m_batchInfo.BLASNodes.size() * sizeof(BvhNode),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	materialBuffer(
		renderContext->allocator, m_batchInfo.materials.size() * sizeof(Material),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	instanceBuffer(
		renderContext->allocator, m_batchInfo.gpuInstances.size() * sizeof(GPUInstance),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	TLASIndexBuffer(
		renderContext->allocator, instances.size() * sizeof(U32),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	TLASNodeBuffer(
		renderContext->allocator, m_sceneTlas.nodesUsed() * sizeof(BvhNode),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	),
	lightBuffer(
		renderContext->allocator, m_batchInfo.lights.size() * sizeof(GPULightData),
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0
	)
{
	assert(renderContext != nullptr);

	VkCommandPoolCreateInfo upCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	upCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	upCreateInfo.queueFamilyIndex = m_renderContext->queues.transferQueue.familyIndex;
	VK_CHECK(vkCreateCommandPool(m_renderContext->device, &upCreateInfo, nullptr, &m_uploadOneshotPool));

	VkFenceCreateInfo upFenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VK_CHECK(vkCreateFence(m_renderContext->device, &upFenceCreateInfo, nullptr, &m_uploadFinishedFence));

	SizeType triBufSize = m_batchInfo.triBuffer.size() * sizeof(Triangle);
	SizeType triExtBufSize = m_batchInfo.triExtBuffer.size() * sizeof(TriExtension);
	SizeType blasIndexBufSize = m_batchInfo.BLASIndices.size() * sizeof(U32);
	SizeType blasNodeBufSize = m_batchInfo.BLASNodes.size() * sizeof(BvhNode);
	SizeType materialBufSize = m_batchInfo.materials.size() * sizeof(Material);
	SizeType instanceBufSize = m_batchInfo.gpuInstances.size() * sizeof(GPUInstance);
	SizeType tlasIndexBufSize = m_batchInfo.gpuInstances.size() * sizeof(U32);
	SizeType tlasNodeBufSize = m_sceneTlas.nodesUsed() * sizeof(BvhNode);
	SizeType lightBufSize = m_batchInfo.lights.size() * sizeof(GPULightData);

	uploadToGPU(m_batchInfo.triBuffer.data(), triBufSize, globalTriBuffer);
	uploadToGPU(m_batchInfo.triExtBuffer.data(), triExtBufSize, globalTriExtBuffer);
	uploadToGPU(m_batchInfo.BLASIndices.data(), blasIndexBufSize, BLASGlobalIndexBuffer);
	uploadToGPU(m_batchInfo.BLASNodes.data(), blasNodeBufSize, BLASGlobalNodeBuffer);
	uploadToGPU(m_batchInfo.materials.data(), materialBufSize, materialBuffer);
	uploadToGPU(m_batchInfo.gpuInstances.data(), instanceBufSize, instanceBuffer);
	uploadToGPU(m_sceneTlas.indices(), tlasIndexBufSize, TLASIndexBuffer);
	uploadToGPU(m_sceneTlas.nodePool(), tlasNodeBufSize, TLASNodeBuffer);
	uploadToGPU(m_batchInfo.lights.data(), lightBufSize, lightBuffer);
}

GPUScene::~GPUScene()
{
	vkDeviceWaitIdle(m_renderContext->device);
	vkDestroyFence(m_renderContext->device, m_uploadFinishedFence, nullptr);
	vkDestroyCommandPool(m_renderContext->device, m_uploadOneshotPool, nullptr);
}

void GPUScene::update(F32 deltaTime)
{
	Instance& instance = m_sceneTlas.instance(3);
	instance.setTransform(glm::rotate(instance.transform(), 1.0f * deltaTime, static_cast<glm::vec3>(WORLD_UP)));

	m_sceneTlas.refit();
	m_batchInfo = GPUBatcher::createBatchInfo(m_sceneTlas.instances());	// XXX: is rebatching fast enough for realtime use with larger scenes?

	SizeType instanceBufSize = m_batchInfo.gpuInstances.size() * sizeof(GPUInstance);
	SizeType tlasIndexBufSize = m_batchInfo.gpuInstances.size() * sizeof(U32);
	SizeType tlasNodeBufSize = m_sceneTlas.nodesUsed() * sizeof(BvhNode);

	uploadToGPU(m_batchInfo.gpuInstances.data(), instanceBufSize, instanceBuffer);
	uploadToGPU(m_sceneTlas.indices(), tlasIndexBufSize, TLASIndexBuffer);
	uploadToGPU(m_sceneTlas.nodePool(), tlasNodeBufSize, TLASNodeBuffer);
}

void GPUScene::uploadToGPU(const void* data, SizeType size, Buffer& target)
{
	VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocInfo.commandPool = m_uploadOneshotPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	
	VkCommandBuffer oneshot = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateCommandBuffers(m_renderContext->device, &allocInfo, &oneshot));

	Buffer cpuBuffer = Buffer(
		m_renderContext->allocator, size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_HOST
	);
	cpuBuffer.copyToBuffer(size, data);

	VkCommandBufferBeginInfo osBegin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	osBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	osBegin.pInheritanceInfo = nullptr;
	VK_CHECK(vkBeginCommandBuffer(oneshot, &osBegin));

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(oneshot, cpuBuffer.handle(), target.handle(), 1, &copyRegion);

	VK_CHECK(vkEndCommandBuffer(oneshot));

	VkSubmitInfo osSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	osSubmit.commandBufferCount = 1;
	osSubmit.pCommandBuffers = &oneshot;
	VK_CHECK(vkQueueSubmit(m_renderContext->queues.transferQueue.handle, 1, &osSubmit, m_uploadFinishedFence));
	VK_CHECK(vkWaitForFences(m_renderContext->device, 1, &m_uploadFinishedFence, VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(m_renderContext->device, 1, &m_uploadFinishedFence));

	vkFreeCommandBuffers(m_renderContext->device, m_uploadOneshotPool, 1, &oneshot);
}
