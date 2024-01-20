#include "window_manager.h"

#define GLFW_INCLUDE_NONE

#include <cassert>
#include <GLFW/glfw3.h>

#include "types.h"

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

bool WindowManager::windowIsMinimized(GLFWwindow* window) const
{
    assert(window != nullptr);
    I32 width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    return width == 0 || height == 0;
}

void WindowManager::pollEvents() const
{
    glfwPollEvents();
}
