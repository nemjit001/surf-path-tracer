#include "window_manager.h"

WindowManager::WindowManager()
{
    glfwInit();
}

WindowManager::~WindowManager()
{
    glfwTerminate();
}

GLFWwindow* WindowManager::createWindow(const char* title, U32 width, U32 height)
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    return glfwCreateWindow(width, height, title, nullptr, nullptr);
}
