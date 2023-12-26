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
