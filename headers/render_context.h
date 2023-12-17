#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"

struct GPUQueues
{
    VkQueue transferQueue;
    VkQueue computeQueue;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

class RenderContext
{
public:
	RenderContext(GLFWwindow* window);

	~RenderContext();

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    RenderContext(RenderContext&& other) noexcept;
    RenderContext& operator=(RenderContext&& other) noexcept;

    void getFramebufferSize(U32& width, U32& height);

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    void release();

private:
    GLFWwindow* m_window;
    vkb::Instance m_instance;
    VkSurfaceKHR m_renderSurface;
    vkb::PhysicalDevice m_gpu;  

public:
    vkb::Device device;
    vkb::Swapchain swapchain;
    std::vector<VkImageView> swapImageViews;
    VmaAllocator allocator;
    GPUQueues queues;
};
