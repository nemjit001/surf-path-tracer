#include "camera.h"

#include "ray.h"
#include "surf_math.h"
#include "types.h"

#define FOCAL_LENGTH	1.0f
#define VIEWPORT_HEIGHT	2.0f

Camera::Camera(Float3 position, Float3 target, U32 screenWidth, U32 screenHeight)
	:
	position(position),
	forward(0.0f),
	up(0.0f),
	screenWidth(static_cast<F32>(screenWidth)),
	screenHeight(static_cast<F32>(screenWidth)),
	viewPlane{}
{
	forward = glm::normalize(target - position);
	Float3 right = glm::normalize(glm::cross(WORLD_UP, forward));
	up = glm::normalize(glm::cross(forward, right));

	generateViewPlane();
}

void Camera::generateViewPlane()
{
	const F32 aspectRatio = this->screenWidth / this->screenHeight;
	const F32 viewportWidth = aspectRatio * VIEWPORT_HEIGHT;

	const Float3 uVector = right() * viewportWidth;
	const Float3 vVector = -1.0f * up * VIEWPORT_HEIGHT;

	viewPlane.topLeft = position + (forward * FOCAL_LENGTH) - (0.5f * uVector) - (0.5f * vVector);
	viewPlane.uVector = uVector;
	viewPlane.vVector = vVector;
}
