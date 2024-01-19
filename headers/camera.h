#pragma once

#include "ray.h"
#include "surf_math.h"
#include "types.h"

#define WORLD_FORWARD	Float3(0.0f, 0.0f, -1.0f)
#define WORLD_RIGHT		Float3(1.0f, 0.0f, 0.0f)
#define WORLD_UP		Float3(0.0f, 1.0f, 0.0f)

// GLSL UBO format for camera data
struct CameraUBO
{
	ALIGN(16) Float3 position;
	ALIGN(16) Float3 up, fwd, right;
	ALIGN(16) Float3 firstPixel, uVector, vVector;
	ALIGN(8)  Float2 resolution;
	ALIGN(4)  F32 focalLength, defocusAngle;
};

struct ViewPlane
{
	Float3 firstPixel;
	Float3 uVector;
	Float3 vVector;
};

class Camera
{
public:
	Camera(Float3 position, Float3 target, U32 screenWidth, U32 screenHeight, F32 fovY, F32 focalLength = 1.5f, F32 defocusAngle = 0.0f);

	inline Float3 right() const;

	inline Ray getPrimaryRay(U32& seed, F32 x, F32 y);

	void generateViewPlane();

private:
	inline Float3 sampleDefocusDisk(U32& seed);

public:
	Float3 position;
	Float3 forward;
	Float3 up;
	F32 screenWidth;
	F32 screenHeight;
	F32 fovY;
	F32 focalLength;
	F32 defocusAngle;
	ViewPlane viewPlane;
};

Float3 Camera::right() const
{
	return up.cross(forward).normalize();
}

Ray Camera::getPrimaryRay(U32& seed, F32 x, F32 y)
{
	const F32 u = x * (1.0f / screenWidth);
	const F32 v = y * (1.0f / screenHeight);

	const Float3 origin = defocusAngle == 0.0f ? position : position + sampleDefocusDisk(seed);
	const Float3 planePosition = viewPlane.firstPixel + u * viewPlane.uVector + v * viewPlane.vVector;
	Float3 direction = (planePosition - origin).normalize();

	return Ray(origin, direction);
}

inline Float3 Camera::sampleDefocusDisk(U32& seed)
{
	const F32 radius = focalLength * tanf(radians(defocusAngle / 2.0f));
	const Float3 u = right() * radius;
	const Float3 v = -1.0f * up * radius;
	Float2 sample = Float2(0);

	do
	{
		sample = Float2(
			randomRange(seed, -1.0f, 1.0f),
			randomRange(seed, -1.0f, 1.0f)
		);
	} while (sample.dot(sample) > 1.0f);

	return sample.u * u + sample.v * v;
}
