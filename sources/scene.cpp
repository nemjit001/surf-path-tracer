#include "scene.h"

#include <cassert>
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

bool Scene::intersect(Ray& ray) const
{
	return m_sceneTlas.intersect(ray);
}

const Instance& Scene::hitInstance(SizeType instanceIndex) const
{
	return m_sceneTlas.instance(instanceIndex);
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
			return alpha * m_background.colorB + (1.0f - alpha) * m_background.colorA;
		}
	default:
		break;
	}

	return RgbColor(0.0f);	// Default to black
}
