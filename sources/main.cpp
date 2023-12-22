#include "surf.h"

#include <cstdio>

#include "bvh.h"
#include "camera.h"
#include "mesh.h"
#include "render_context.h"
#include "renderer.h"
#include "scene.h"
#include "surf_math.h"
#include "pixel_buffer.h"
#include "timer.h"
#include "types.h"
#include "window_manager.h"

#ifdef _WIN32
#ifndef NDEBUG

// Windows debug mem leak detection
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#endif
#endif // _WIN32

#define RESOLUTION_SCALE	1.0f
#define	CAMERA_SPEED		2.0f
#define NUM_SMOOTH_FRAMES	20	// Number of frames to smooth FPS / frame timing over

void handleCameraInput(GLFWwindow* window, Camera& camera, F32 deltaTime, bool& updated)
{
	updated = false;

	Float3 forward = camera.forward.normalize();
	Float3 right = WORLD_UP.cross(forward).normalize();
	Float3 up = forward.cross(right).normalize();

	Float3 deltaPosition(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		deltaPosition += 2.0f * CAMERA_SPEED * forward * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		deltaPosition -= 2.0f * CAMERA_SPEED * forward * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		deltaPosition += 2.0f * CAMERA_SPEED * right * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		deltaPosition -= 2.0f * CAMERA_SPEED * right * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
	{
		deltaPosition += 2.0f * CAMERA_SPEED * up * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
	{
		deltaPosition -= 2.0f * CAMERA_SPEED * up * deltaTime;
		updated = true;
	}

	camera.position += deltaPosition;
	Float3 target = camera.position + forward;

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
	{
		target += CAMERA_SPEED * up * deltaTime;
		updated = true;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
	{
		target -= CAMERA_SPEED * up * deltaTime;
		updated = true;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
	{
		target += CAMERA_SPEED * right * deltaTime;
		updated = true;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
	{
		target -= CAMERA_SPEED * right * deltaTime;
		updated = true;
	}

	if (!updated)
		return;

	camera.forward = (target - camera.position).normalize();
	camera.up = forward.cross(right).normalize();
	camera.generateViewPlane();
}

int main()
{
#ifdef _WIN32
#ifndef NDEBUG
	// Enable debug mem heap tracking
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#endif

	WindowManager windowManager;
	GLFWwindow* window = windowManager.createWindow(PROGRAM_NAME, SCR_WIDTH, SCR_HEIGHT);

	RenderContext renderContext(window);
	FramebufferSize resolution = renderContext.getFramebufferSize();

	PixelBuffer resultBuffer(
		static_cast<U32>(resolution.width * RESOLUTION_SCALE),
		static_cast<U32>(resolution.height * RESOLUTION_SCALE)
	);

	Camera worldCam(
		Float3(0.0f, 0.0f, 5.0f),
		Float3(0.0f, 0.0f, 0.0f),
		resultBuffer.width,
		resultBuffer.height
	);

	Mesh testMesh("assets/susanne.obj");
	BvhBLAS testBvh(&testMesh);

	Instance testInstance = Instance{
		&testBvh
	};

	Scene scene(testInstance);
	Renderer renderer(std::move(renderContext), resultBuffer, worldCam, scene);

	// Create frame timer
	Timer frameTimer;

	printf("Initialized Surf\n");

	while (!glfwWindowShouldClose(window))
	{
		static F32 SMOOTH_FRAC = 1.0f / NUM_SMOOTH_FRAMES;
		static F32 AVERAGE_FRAMETIME = 10.0f;
		static F32 ALPHA = 1.0f;

		frameTimer.tick();
		F32 deltaTime  = frameTimer.deltaTime();

		AVERAGE_FRAMETIME = (1.0f - ALPHA) * AVERAGE_FRAMETIME + ALPHA * (deltaTime * 1'000.0f);
		if (ALPHA > SMOOTH_FRAC)
			ALPHA *= 0.5;

		F32 inv_avg_frametime = 1.0f / AVERAGE_FRAMETIME;
		printf("%08.2fms (%08.2ffps)\n", AVERAGE_FRAMETIME, inv_avg_frametime * 1'000.0f);

		// Render frame
		renderer.render(deltaTime);

		// Handle input
		bool cameraUpdated = false;
		handleCameraInput(window, worldCam, deltaTime, cameraUpdated);
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		if (cameraUpdated)
			renderer.clearAccumulator();

		glfwPollEvents();
	}

	printf("Goodbye!\n");
	return 0;
}
