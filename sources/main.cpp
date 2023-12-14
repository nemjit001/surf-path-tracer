#include "surf.hpp"

#define GLFW_INCLUDE_VULKAN

#include <cstdio>
#include <GLFW/glfw3.h>

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, PROGRAM_NAME, nullptr, nullptr);

	printf("Initialized Surf\n");

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwTerminate();

	printf("Goodbye!\n");
	return 0;
}
