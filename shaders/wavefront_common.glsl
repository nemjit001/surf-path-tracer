#ifndef GLSL_WF_COMMON
#define GLSL_WF_COMMON

#define F32_FAR_AWAY	1e30
#define F32_EPSILON		1e-5

#define F32_PI			3.14159265358979323846264
#define F32_INV_PI		0.31830988618379067153777
#define F32_INV_2PI		0.15915494309189533576888
#define F32_2PI			6.28318530717958647692528

#define UNSET_IDX		~0

// Background type enum vals in scene UBO
#define SCENE_BG_TYPE_SOLID		0
#define SCENE_BG_TYPE_GRADIENT	1

struct SceneBackground
{
	uint type;
	vec3 solidColor;
	vec3 gradientA;
	vec3 gradientB;
};

struct Material
{
	vec3 emissionColor;
	float reflectivity;
	float refractivity;
	float indexOfRefraction;
	float emissionStrength;
	vec3 albedo;
	vec3 absorption;
};

struct RayHit
{
	uint instanceIdx;
	uint primitiveIdx;
	vec2 hitCoords;
};

struct Ray
{
	vec3 origin;
	vec3 direction;
	float depth;
	bool inMedium;
	vec3 transmission;
	uint pixelIdx;		// Pixel index for ray into out buffer
	RayHit hit;
};

uint WangHash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	return seed;
}

uint initSeed(uint seed)
{
	return WangHash((seed + 1) * 0x11);
}

uint randomU32(inout uint seed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

float randomF32(inout uint seed)
{
	return randomU32(seed) * 2.3283064365387e-10;
}

float randomRange(inout uint seed, float minRange, float maxRange)
{
	return (randomF32(seed) * (maxRange - minRange)) + minRange;
}

vec3 diffuseReflect(inout uint seed, vec3 normal)
{
	vec3 direction = vec3(0);

	do
	{
		direction = vec3(
			randomRange(seed, -1.0, 1.0),
			randomRange(seed, -1.0, 1.0),
			randomRange(seed, -1.0, 1.0)
		);
	} while(dot(direction, direction) > 1.0);

	if (dot(direction, normal) < 0.0)
	{
		direction *= -1.0;
	}

	return normalize(direction);
}

vec3 diffuseReflectCosineWeighted(inout uint seed, vec3 normal)
{
	// FIXME: implement this
	return diffuseReflect(seed, normal);
}

vec3 sampleSkyColor(Ray ray, SceneBackground background)
{
	if (background.type == SCENE_BG_TYPE_SOLID)
		return background.solidColor;

	if (background.type == SCENE_BG_TYPE_GRADIENT)
	{
		float alpha = 0.5 * (1.0 + ray.direction.y);
		return alpha * background.gradientB + (1.0 - alpha) * background.gradientA;
	}

	return vec3(0);	// Default to black
}

bool materialIsLight(Material material)
{
	return material.emissionStrength > 0.0
	|| material.emissionColor.x > 0.0
	|| material.emissionColor.y > 0.0
	|| material.emissionColor.z > 0.0;
}

vec3 materialEmittance(Material material)
{
	return material.emissionStrength * material.emissionColor;
}

Ray newRay(vec3 origin, vec3 direction)
{
	return Ray(
		origin,
		direction,
		F32_FAR_AWAY,
		false,
		vec3(1),
		UNSET_IDX,
		RayHit(UNSET_IDX, UNSET_IDX, vec2(0))
	);
}

bool depthInBounds(float depth, float maxDepth)
{
	return F32_EPSILON <= depth && depth < maxDepth;
}

vec3 rayHitPosition(Ray ray)
{
	return ray.origin + ray.depth * ray.direction;
}

#endif
