#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include "types.h"

class Renderer
{
public:
    Renderer();

    void init(GLFWwindow* window);

    void destroy();

    void render(F32 deltaTime);

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

private:
    GLFWwindow* m_window;
    vkb::Instance m_instace;
    VkSurfaceKHR m_renderSurface;
    vkb::PhysicalDevice m_gpu;
    vkb::Device m_device;
    vkb::Swapchain m_swapchain;
};
