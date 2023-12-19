#include "surf.h"

#include <cstdio>

#include "camera.h"
#include "render_context.h"
#include "renderer.h"
#include "surf_math.h"
#include "pixel_buffer.h"
#include "timer.h"
#include "types.h"
#include "window_manager.h"

#define RESOLUTION_SCALE	1.0

#define NUM_SMOOTH_FRAMES	20	// Number of frames to smooth FPS / frame timing over

int main()
{
	WindowManager windowManager;
	GLFWwindow* window = windowManager.createWindow(PROGRAM_NAME, SCR_WIDTH, SCR_HEIGHT);

	RenderContext renderContext(window);
	FramebufferSize resolution = renderContext.getFramebufferSize();

	PixelBuffer resultBuffer(
		static_cast<U32>(resolution.width * RESOLUTION_SCALE),
		static_cast<U32>(resolution.height * RESOLUTION_SCALE)
	);

	Camera worldCam(
		Float3(0.0f, 0.0f, -2.0f),
		Float3(0.0f, 0.0f, 0.0f),
		resultBuffer.width,
		resultBuffer.height
	);

	Renderer renderer(std::move(renderContext), resultBuffer, &worldCam);

	// TODO: load scene

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

		renderer.render(deltaTime);
		glfwPollEvents();
	}

	printf("Goodbye!\n");
	return 0;
}
