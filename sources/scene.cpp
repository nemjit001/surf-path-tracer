#include "scene.h"

#include <cassert>
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

GPUScene::GPUScene(RenderContext* renderContext, SceneBackground background, std::vector<Instance> instances)
	:
	Scene(background, instances)
{
	assert(renderContext != nullptr);

	SizeType triBufferSize = 0;
	SizeType triExBufferSize = 0;
	SizeType TLASIdxBufferSize = instances.size() * sizeof(U32);
	SizeType TLASNodeBufferSize = m_sceneTlas.nodesUsed() * sizeof(BvhNode);

	// GPU mesh list
	std::vector<GPUMesh> gpuMeshes;
	std::vector<U32> globalBLASIndices;
	std::vector<BvhNode> globalBLASNodes;
	std::vector<Material> materialList;
	std::vector<GPUInstance> gpuInstances;

	// Handle unique meshes & materials
	{
		std::set<const Mesh*> meshes;
		std::set<const BvhBLAS*> blasses;
		std::set<const Material*> materials;

		for (auto const& instance : instances)
		{
			meshes.insert(instance.bvh->mesh());
			blasses.insert(instance.bvh);
			materials.insert(instance.material);
		}

		// Create GPU meshes & calculate total buffer size
		for (auto const& mesh : meshes)
		{
			gpuMeshes.push_back(GPUMesh(renderContext, *mesh));
			triBufferSize += gpuMeshes.back().triBuffer.size();
			triExBufferSize += gpuMeshes.back().triBuffer.size();
		}

		// Gather blas nodes & indices
		for (auto const& blas : blasses)
		{
			globalBLASIndices.insert(globalBLASIndices.end(), blas->indices(), blas->indices() + blas->triCount());
			globalBLASNodes.insert(globalBLASNodes.end(), blas->nodePool(), blas->nodePool() + blas->nodesUsed());
		}

		// Gather materials
		for (auto const& material : materials)
		{
			materialList.push_back(*material);
		}

		// TODO: For every GPU instance -> calculate global tri offset, bvh idx + node start & count, material idx
	}

	// TODO: allocate buffers & upload data to GPU
}
