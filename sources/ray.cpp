#include "ray.h"

Ray::Ray(Float3 origin, Float3 direction)
	:
	origin(origin),
	depth(F32_FAR_AWAY),
	direction(direction),
	metadata{
		UNSET_INDEX,
		UNSET_INDEX,
		Float2(0.0f)
	}
{
	//
}
