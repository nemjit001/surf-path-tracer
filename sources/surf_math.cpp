#include "surf_math.h"

U32 RgbaToU32(RgbaColor color)
{
	// TODO: implement color SIMD implementation
	U32 r = static_cast<U32>(color.r * 255.99f);
	U32 g = static_cast<U32>(color.g * 255.99f);
	U32 b = static_cast<U32>(color.b * 255.99f);
	U32 a = static_cast<U32>(color.a * 255.99f);

	return ((a & 0xFF) << 24) + ((b & 0xFF) << 16) + ((g & 0xFF) << 8) + (r & 0xFF);
}
