#include "surf.h"

#define GLFW_INCLUDE_NONE

#include <cstdio>
#include <GLFW/glfw3.h>

#include "renderer.h"
#include "timer.h"

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, PROGRAM_NAME, nullptr, nullptr);
	Renderer renderer;
	renderer.init(window);

	// TODO: load scene

	// Create frame timer
	Timer frameTimer;

	printf("Initialized Surf\n");

	while (!glfwWindowShouldClose(window))
	{
		frameTimer.tick();
		F32 deltaTime  = frameTimer.deltaTime();
    	printf("%08.2fms (%08.2ffps)\n", deltaTime * 1'000.0f, 1.0f / deltaTime);

		renderer.render(deltaTime);
		glfwPollEvents();
	}

	renderer.destroy();
	glfwTerminate();

	printf("Goodbye!\n");
	return 0;
}
