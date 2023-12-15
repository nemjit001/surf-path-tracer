#include "surf.h"

#define GLFW_INCLUDE_NONE

#include <cstdio>
#include <GLFW/glfw3.h>

#include "renderer.h"

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, PROGRAM_NAME, nullptr, nullptr);
	Renderer renderer;
	renderer.init(window);

	// TODO: load scene

	printf("Initialized Surf\n");

	while (!glfwWindowShouldClose(window))
	{
		renderer.render(0.0f);
		glfwPollEvents();
	}

	renderer.destroy();
	glfwTerminate();

	printf("Goodbye!\n");
	return 0;
}
