#pragma once

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>

#include "types.h"

class WindowManager
{
public:
    WindowManager();

    ~WindowManager();

    GLFWwindow* createWindow(const char* title, U32 width, U32 height);

    bool windowIsMinimized(GLFWwindow* window) const;

    void pollEvents() const;
};
