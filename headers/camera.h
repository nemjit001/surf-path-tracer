#pragma once

#include "ray.h"
#include "surf_math.h"
#include "types.h"

#define WORLD_FORWARD	Float3(0.0f, 0.0f, -1.0f)
#define WORLD_RIGHT		Float3(1.0f, 0.0f, 0.0f)
#define WORLD_UP		Float3(0.0f, 1.0f, 0.0f)

struct ViewPlane
{
	Float3 firstPixel;
	Float3 uVector;
	Float3 vVector;
};

class Camera
{
public:
	Camera(Float3 position, Float3 target, U32 screenWidth, U32 screenHeight, F32 focalLength = 1.5f);

	inline Float3 right() const;

	inline Ray getPrimaryRay(F32 x, F32 y);

	void generateViewPlane();

public:
	Float3 position;
	Float3 forward;
	Float3 up;
	F32 screenWidth;
	F32 screenHeight;
	F32 focalLength;
	ViewPlane viewPlane;
};

Float3 Camera::right() const
{
	return up.cross(forward).normalize();
}

Ray Camera::getPrimaryRay(F32 x, F32 y)
{
	const F32 u = x * (1.0f / screenWidth);
	const F32 v = y * (1.0f / screenHeight);

	const Float3 planePosition = viewPlane.firstPixel + u * viewPlane.uVector + v * viewPlane.vVector;
	Float3 direction = (planePosition - position).normalize();

	return Ray(position, direction);
}
