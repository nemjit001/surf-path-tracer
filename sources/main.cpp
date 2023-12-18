#include "surf.h"

#include <cstdio>

#include "render_context.h"
#include "renderer.h"
#include "pixel_buffer.h"
#include "timer.h"
#include "types.h"
#include "window_manager.h"

#define NUM_SMOOTH_FRAMES 20	// Number of frames to smooth FPS / frame timing over

int main()
{
	WindowManager windowManager;
	GLFWwindow* window = windowManager.createWindow(PROGRAM_NAME, SCR_WIDTH, SCR_HEIGHT);

	RenderResulution resolution = RenderResulution{
		SCR_WIDTH,
		SCR_HEIGHT
	};

	PixelBuffer resultBuffer(resolution.width, resolution.height);
	RenderContext renderContext(window);
	Renderer renderer(std::move(renderContext), resolution, resultBuffer);

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
