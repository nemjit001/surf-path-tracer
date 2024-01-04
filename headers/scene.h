#pragma once

#include <vector>

#include "bvh.h"
#include "ray.h"
#include "surf_math.h"

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

private:
	SceneBackground m_background;
	BvhTLAS m_sceneTlas;
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
