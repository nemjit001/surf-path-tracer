struct Ray
{
	vec3 origin;
	vec3 direction;
	float depth;
	uint primitiveIndex;
	uint instanceIndex;
	vec2 barycentric;
};

uint WangHash(inout uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	return seed;
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

	if (dot(direction, normal) < 0.0f)
	{
		direction *= -1.0;
	}

	return normalize(direction);
}
