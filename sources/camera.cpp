#include "camera.h"

#include "ray.h"
#include "surf_math.h"
#include "types.h"

#define VIEWPORT_HEIGHT	2.0f

Camera::Camera(Float3 position, Float3 target, U32 screenWidth, U32 screenHeight, F32 fovY, F32 focalLength, F32 defocusAngle)
	:
	position(position),
	forward(0.0f),
	up(0.0f),
	screenWidth(static_cast<F32>(screenWidth)),
	screenHeight(static_cast<F32>(screenHeight)),
	fovY(fovY),
	focalLength(focalLength),
	defocusAngle(defocusAngle),
	viewPlane{}
{
	forward = (target - position).normalize();
	Float3 right = WORLD_UP.cross(forward).normalize();
	up = forward.cross(right).normalize();

	generateViewPlane();
}

void Camera::generateViewPlane()
{
	const F32 heightScale = tanf(radians(fovY) / 2.0f);

	const F32 aspectRatio = this->screenWidth / this->screenHeight;
	const F32 viewportHeight = VIEWPORT_HEIGHT * heightScale * focalLength;
	const F32 viewportWidth = aspectRatio * viewportHeight;

	const Float3 uVector = right() * viewportWidth;
	const Float3 vVector = -1.0f * up * viewportHeight;

	const Float3 uDelta = uVector / screenWidth;
	const Float3 vDelta = vVector / screenHeight;

	const Float3 topLeft = position + (forward * focalLength) - (0.5f * uVector) - (0.5f * vVector);
	viewPlane.firstPixel = topLeft + 0.5f * (uDelta + vDelta);
	viewPlane.uVector = uVector;
	viewPlane.vVector = vVector;
}
