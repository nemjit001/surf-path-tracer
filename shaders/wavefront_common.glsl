#define F32_FAR_AWAY	1e30
#define F32_EPSILON		1e-5

#define F32_PI			3.14159265358979323846264
#define F32_INV_PI		0.31830988618379067153777
#define F32_INV_2PI		0.15915494309189533576888
#define F32_2PI			6.28318530717958647692528

#define UNSET_IDX		~0

struct Ray
{
	vec3 origin;
	vec3 direction;
	float depth;
	uint pixelIdx;		// Pixel index for ray into out buffer
	uint instanceIdx;	// Index into TLAS instance list
	uint primitiveIdx;	// Index into BLAS primitive list
	vec2 hitCoords;		// Barycentric hit coordinates
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

bool rayDepthInBounds(Ray ray, float depth)
{
	return F32_EPSILON < depth && depth < ray.depth;
}

vec3 rayHitPosition(Ray ray)
{
	return ray.origin + ray.depth * ray.direction;
}

// XXX: Temp sphere stuff for test scene
#define SPHERE_POSITION	vec3(0, 0, 0)
#define SPHERE_RADIUS	1.0

vec3 sphereNormal(vec3 hitPosition)
{
	return (hitPosition - SPHERE_POSITION) / SPHERE_RADIUS;
}

bool sphereIntersect(inout Ray ray)
{
	vec3 oc = ray.origin - SPHERE_POSITION;
	float a = dot(ray.direction, ray.direction);
	float halfB = dot(oc, ray.direction);
	float c = dot(oc, oc) - (SPHERE_RADIUS * SPHERE_RADIUS);

	float d = (halfB * halfB) - (a * c);
	if (d < 0.0)
		return false;

	float depth = (-halfB - sqrt(d)) / a;
	if (!rayDepthInBounds(ray, depth))
	{
		depth = (-halfB + sqrt(d)) / a;
		if (!rayDepthInBounds(ray, depth))
		{
			return false;
		}
	}

	ray.depth = depth;
	return true;
}
