#include "surf_math.h"

#define GLM_FORCE_LEFT_HANDED

#include <glm/glm.hpp>
#include <cmath>

#include "types.h"

static U32 RAND_SEED = 0x12345678;

U32 RgbaToU32(const RgbaColor& color)
{
	// TODO: implement color SIMD implementation
	U32 r = static_cast<U32>(color.r * 255.99f);
	U32 g = static_cast<U32>(color.g * 255.99f);
	U32 b = static_cast<U32>(color.b * 255.99f);
	U32 a = static_cast<U32>(color.a * 255.99f);

	return ((a & 0xFF) << 24) + ((b & 0xFF) << 16) + ((g & 0xFF) << 8) + (r & 0xFF);
}

U32 randomU32()
{
	RAND_SEED ^= RAND_SEED << 13;
	RAND_SEED ^= RAND_SEED >> 17;
	RAND_SEED ^= RAND_SEED << 5;
	return RAND_SEED;
}

U32 randomU32(U32& seed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

F32 randomF32()
{
	return randomU32() * 2.3283064365387e-10f;
}

F32 randomF32(U32& seed)
{
	return randomU32(seed) * 2.3283064365387e-10f;
}

F32 randomRange(F32 range)
{
	return randomF32() * range;
}

F32 randomRange(U32& seed, F32 range)
{
	return randomF32(seed) * range;
}

Float3 randomOnHemisphere(const Float3& normal)
{
	Float3 direction = Float3(
		randomF32() * 2.0f - 1.0f,
		randomF32() * 2.0f - 1.0f,
		randomF32() * 2.0f - 1.0f
	);

	while (glm::dot(direction, direction) > 1.0f)
	{
		direction = Float3(
			randomF32() * 2.0f - 1.0f,
			randomF32() * 2.0f - 1.0f,
			randomF32() * 2.0f - 1.0f
		);
	}

	if (glm::dot(direction, normal) < 0.0f)
		direction *= -1.0f;

	return glm::normalize(direction);
}

bool depthInBounds(F32 depth, F32 maxDepth)
{
	return F32_EPSILON <= depth && depth < maxDepth;
}
