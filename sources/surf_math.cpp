#include "surf_math.h"

#include <cmath>
#include <immintrin.h>

#include "camera.h"
#include "types.h"

#define U32_TO_FLOAT_SCALE	2.3283064365387e-10f

static U32 RAND_SEED = 0x12345678;

U32 RgbaToU32(const RgbaColor& color)
{
#if 0
	U32 r = static_cast<U32>(color.r * 255.00f);
	U32 g = static_cast<U32>(color.g * 255.00f);
	U32 b = static_cast<U32>(color.b * 255.00f);
	U32 a = static_cast<U32>(color.a * 255.00f);

	return ((a & 0xFF) << 24) + ((b & 0xFF) << 16) + ((g & 0xFF) << 8) + (r & 0xFF);
#else
	static __m128 S4 = _mm_set1_ps(255.0f);
	__m128 a = _mm_load_ps(color.xyzw);
	__m128i b = _mm_cvtps_epi32(_mm_mul_ps(a, S4));
	__m128i b32 = _mm_packus_epi32(b, b);
	return static_cast<U32>(_mm_cvtsi128_si32(_mm_packus_epi16(b32, b32)));
#endif
}

U32 wangHash(U32 seed)
{
	assert(seed != 0);

	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	return seed;
}

U32 initSeed(U32 seed)
{
	return wangHash((seed + 1) * 0x11);
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
	return randomU32() * U32_TO_FLOAT_SCALE;
}

F32 randomF32(U32& seed)
{
	return randomU32(seed) * U32_TO_FLOAT_SCALE;
}

F32 randomRange(F32 min, F32 max)
{
	F32 range = max - min;
	return (randomF32() * range) + min;
}

F32 randomRange(U32& seed, F32 min, F32 max)
{
	F32 range = max - min;
	return (randomF32(seed) * range) + min;
}

U32 randomRange(U32 min, U32 max)
{
	return (randomU32() + min) % max;
}

U32 randomRange(U32& seed, U32 min, U32 max)
{
	return (randomU32(seed) + min) % max;
}

Float3 randomOnHemisphere(U32& seed, const Float3& normal)
{
	Float3 direction = Float3(0.0f);

	do
	{
		direction = Float3(
			randomRange(seed, -1.0f, 1.0f),
			randomRange(seed, -1.0f, 1.0f),
			randomRange(seed, -1.0f, 1.0f)
		);
	} while (direction.dot(direction) > 1.0f);

	if (direction.dot(normal) < 0.0f)
		direction *= -1.0f;

	return direction.normalize();
}

Float3 randomOnHemisphereCosineWeighted(U32& seed, const Float3& normal)
{
	F32 r0 = randomF32(seed);
	F32 r1 = randomF32(seed);
	F32 r = sqrtf(r0);
	F32 theta = F32_2PI * r1;
	Float3 direction(r * cosf(theta), r * sinf(theta), sqrtf(1.0f - r0));

	static const F32 X_MAX = 1.0f - F32_EPSILON;
	Float3 tmp = (fabs(normal.x) > X_MAX) ? WORLD_UP : WORLD_RIGHT;
	Float3 B = normal.cross(tmp).normalize();
	Float3 T = B.cross(normal);
	Float3 outDirection = direction.x * T + direction.y * B + direction.z * normal;

	if (outDirection.dot(normal) == 0.0f)
		return randomOnHemisphereCosineWeighted(seed, normal);	// retry (sometimes R.N is 0 for some reason)

	return outDirection;
}
