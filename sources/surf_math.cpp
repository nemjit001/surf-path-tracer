#include "surf_math.h"

#include <cmath>

#include "types.h"

static U32 RAND_SEED = 0x12345678;

U32 RgbaToU32(const RgbaColor& color)
{
	// TODO: implement color SIMD implementation
	U32 r = static_cast<U32>(color.r * 255.00f);
	U32 g = static_cast<U32>(color.g * 255.00f);
	U32 b = static_cast<U32>(color.b * 255.00f);
	U32 a = static_cast<U32>(color.a * 255.00f);

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

F32 randomRange(F32 min, F32 max)
{
	F32 range = max - min;
	return (randomF32() * range) + min;
}

F32 randomRange(U32& seed, F32 range)
{
	return randomF32(seed) * range;
}

Float3 randomOnHemisphere(const Float3& normal)
{
	Float3 direction = Float3(0.0f);

	do
	{
		direction = Float3(
			randomRange(-1.0f, 1.0f),
			randomRange(-1.0f, 1.0f),
			randomRange(-1.0f, 1.0f)
		);
	} while (direction.dot(direction) > 1.0f);

	if (direction.dot(normal) < 0.0f)
		direction *= -1.0f;

	return direction.normalize();
}

Float3 reflect(const Float3& direction, Float3& normal)
{
	return direction - 2.0f * normal.dot(direction) * normal;
}

bool depthInBounds(F32 depth, F32 maxDepth)
{
	return F32_EPSILON <= depth && depth < maxDepth;
}

Float3 float3Min(const Float3& a, const Float3& b)
{
	F32 x = fminf(a.x, b.x);
	F32 y = fminf(a.y, b.y);
	F32 z = fminf(a.z, b.z);

	return Float3(x, y, z);
}

Float3 float3Max(const Float3& a, const Float3& b)
{
	F32 x = fmaxf(a.x, b.x);
	F32 y = fmaxf(a.y, b.y);
	F32 z = fmaxf(a.z, b.z);

	return Float3(x, y, z);
}
