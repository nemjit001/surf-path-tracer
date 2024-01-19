#pragma once

#include <vector>
#include <vulkan/vulkan.h>

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
	ALIGN(4)  BackgroundType type;
	ALIGN(16) RgbColor color;
	struct {
		ALIGN(16) RgbColor colorA;
		ALIGN(16) RgbColor colorB;
	} gradient;
};

class IScene
{
public:
	virtual const SceneBackground& backgroundSettings() const = 0;

	virtual void update(F32 deltaTime) = 0;
};

class Scene
	:
	public IScene
{
public:
	Scene(SceneBackground background, std::vector<Instance> instances);

	virtual ~Scene() = default;

	inline bool intersect(Ray& ray) const { return m_sceneTlas.intersect(ray); }

	inline bool intersectAny(Ray& ray) const { return m_sceneTlas.intersectAny(ray); }

	inline const Instance& hitInstance(SizeType instanceIndex) { return m_sceneTlas.instance(instanceIndex); }

	inline const U32 lightCount() const { return static_cast<U32>(m_lightIndices.size()); }

	inline const Instance& sampleLights(U32& seed) { return m_sceneTlas.instance(m_lightIndices[randomRange(seed, 0, lightCount())]); }

	RgbColor sampleBackground(const Ray& ray) const;

	virtual inline const SceneBackground& backgroundSettings() const override { return m_background; }

	virtual void update(F32 deltaTime) override;

private:
	SceneBackground m_background;
	BvhTLAS m_sceneTlas;
	std::vector<U32> m_lightIndices;
};

struct GPULightData
{
	ALIGN(4) U32 lightInstanceIdx;
	ALIGN(4) U32 primitiveCount;
};

struct GPUBatchInfo
{
	std::vector<Triangle> triBuffer;
	std::vector<TriExtension> triExtBuffer;
	std::vector<U32> BLASIndices;
	std::vector<BvhNode> BLASNodes;
	std::vector<Material> materials;
	std::vector<GPUInstance> gpuInstances;
	std::vector<GPULightData> lights;
};

class GPUBatcher
{
public:
	static const GPUBatchInfo createBatchInfo(const std::vector<Instance>& instances);
};

class GPUScene
	: public IScene
{
public:
	GPUScene(RenderContext* renderContext, SceneBackground background, std::vector<Instance> instances);

	virtual ~GPUScene();

	virtual inline const SceneBackground& backgroundSettings() const override { return m_background; }

	virtual void update(F32 deltaTime) override;

private:
	void uploadToGPU(const void* data, SizeType size, Buffer& target);

private:
	RenderContext* m_renderContext;
	VkCommandPool m_uploadOneshotPool = VK_NULL_HANDLE;
	VkFence m_uploadFinishedFence 	= VK_NULL_HANDLE;
	GPUBatchInfo m_batchInfo;
	SceneBackground m_background;
	BvhTLAS m_sceneTlas;

public:
	Buffer globalTriBuffer;			// Global mesh triangle buffer.
	Buffer globalTriExtBuffer;		// Global mesh tri extension data buffer.
	Buffer BLASGlobalIndexBuffer;	// All BLASses will be stored in 1 buffer,
	Buffer BLASGlobalNodeBuffer;	// with nodes & indices stored in 2 SSBOs.
	Buffer materialBuffer;			// Materials are stored in a single SSBO.
	Buffer instanceBuffer;			// The Instance buffer contains GPUInstances with offsets into global BLAS buffers.
	Buffer TLASIndexBuffer;			// The TLAS index buffer containes indices into the instance buffer.
	Buffer TLASNodeBuffer;			// The TLAS Node buffer contains TLAS BVH nodes.
	Buffer lightBuffer;				// The light buffer contains needed data for all lights in the scene.
};
