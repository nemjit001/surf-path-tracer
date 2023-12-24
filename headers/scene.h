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
	union {
		RgbColor color;
		struct { RgbColor colorA; RgbColor colorB; };
	};
};

class Scene
{
public:
	Scene(SceneBackground background, std::vector<Instance> instances);

	bool intersect(Ray& ray) const;

	const Instance& hitInstance(SizeType instanceIndex) const;

	RgbColor sampleBackground(const Ray& ray) const;

private:
	SceneBackground m_background;
	BvhTLAS m_sceneTlas;
};
