#ifndef GLSL_WF_COMMON
#define GLSL_WF_COMMON

#define F32_FAR_AWAY	1e30
#define F32_EPSILON		1e-5

#define F32_PI			3.14159265358979323846264
#define F32_INV_PI		0.31830988618379067153777
#define F32_INV_2PI		0.15915494309189533576888
#define F32_2PI			6.28318530717958647692528

#define UNSET_IDX		~0

#define WORLD_FORWARD	vec3(0, 0, 1)
#define WORLD_RIGHT		vec3(-1, 0, 0)
#define WORLD_UP 		vec3(0, 1, 0)

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
	float emissionStrength;
	float reflectivity;
	float refractivity;
	float indexOfRefraction;
	vec3 emissionColor;
	vec3 albedo;
	vec3 absorption;
};

struct RayState
{
	bool inMedium;
	bool lastSpecular;
	uint pixelIdx;
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
	vec3 transmission;
	vec3 energy;
	RayState state;
	RayHit hit;
};

struct ShadowRayMetadata
{
	Ray shadowRay;
	vec3 L, LN;
	vec3 brdf, N;
	uint hitInstanceIdx, lightInstanceIdx;
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

uint randomRangeU32(inout uint seed, uint minRange, uint maxRange)
{
	return (randomU32(seed) + minRange) % maxRange;
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
	vec3 outDirection = vec3(0);

	do
	{
		float r0 = randomF32(seed);
		float r1 = randomF32(seed);
		float r = sqrt(r0);
		float theta = F32_2PI * r1;
		vec3 direction = vec3(r * cos(theta), r * sin(theta), sqrt(1.0 - r0));

		vec3 tmp = (abs(normal.x) > 0.99) ? WORLD_UP : WORLD_RIGHT;
		vec3 B = normalize(cross(normal, tmp));
		vec3 T = cross(B, normal);
		outDirection = direction.x * T + direction.y * B + direction.z * normal;
	} while (dot(outDirection, normal) == 0.0);

	return outDirection;
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
		vec3(1),
		vec3(0),
		RayState(false, true, UNSET_IDX),
		RayHit(UNSET_IDX, UNSET_IDX, vec2(0))
	);
}

void copyRayMetadata(inout Ray new, Ray old)
{
	new.transmission = old.transmission;
	new.energy = old.energy;
	new.state = old.state;
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
