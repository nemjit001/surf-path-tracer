#pragma once

#include <vector>

#include "bvh.h"
#include "ray.h"
#include "render_context.h"
#include "surf_math.h"
#include "vk_layer/buffer.h"

enum class BackgroundType
{
	SolidColor,
	ColorGradient
};

struct SceneBackground
{
	BackgroundType type;
	RgbColor color;
	struct { RgbColor colorA; RgbColor colorB; } gradient;
};

class Scene
{
public:
	Scene(SceneBackground background, std::vector<Instance> instances);

	inline bool intersect(Ray& ray) const;

	inline const Instance& hitInstance(SizeType instanceIndex);

	inline const SceneBackground& backgroundSettings() const;

	RgbColor sampleBackground(const Ray& ray) const;

	void update(F32 deltaTime);

protected:
	SceneBackground m_background;
	BvhTLAS m_sceneTlas;
};

struct GPUBatchInfo
{
	std::vector<Triangle> triBuffer;
	std::vector<TriExtension> triExtBuffer;
	std::vector<U32> BLASIndices;
	std::vector<BvhNode> BLASNodes;
	std::vector<Material> materials;
	std::vector<GPUInstance> gpuInstances;
};

class GPUBatcher
{
public:
	static const GPUBatchInfo createBatchInfo(const std::vector<Instance>& instances);
};

class GPUScene
	: public Scene
{
public:
	GPUScene(RenderContext* renderContext, SceneBackground background, std::vector<Instance> instances);

public:
	GPUBatchInfo batchInfo;			// Batching data for instanes
	Buffer globalTriBuffer;			// Global mesh triangle buffer.
	Buffer globalTriExtBuffer;		// Global mesh tri extension data buffer.
	Buffer BLASGlobalIndexBuffer;	// All BLASses will be stored in 1 buffer,
	Buffer BLASGlobalNodeBuffer;	// with nodes & indices stored in 2 SSBOs.
	Buffer materialBuffer;			// Materials are stored in a single SSBO.
	Buffer instanceBuffer;			// The Instance buffer contains GPUInstances with offsets into global BLAS buffers.
	Buffer TLASIndexBuffer;			// The TLAS index buffer containes indices into the instance buffer.
	Buffer TLASNodeBuffer;			// The TLAS Node buffer contains TLAS BVH nodes.
};

bool Scene::intersect(Ray& ray) const
{
	return m_sceneTlas.intersect(ray);
}

const Instance& Scene::hitInstance(SizeType instanceIndex)
{
	return m_sceneTlas.instance(instanceIndex);
}

const SceneBackground& Scene::backgroundSettings() const
{
	return m_background;
}
