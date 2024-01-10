#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"

#ifndef NDEBUG
#define SURF_DEBUG_REPORT
#endif

struct FramebufferSize
{
    U32 width;
    U32 height;
};

struct GPUQueue
{
    U32 familyIndex;
    VkQueue handle;
};

struct GPUQueues
{
    GPUQueue transferQueue;
    GPUQueue computeQueue;
    GPUQueue graphicsQueue;
    GPUQueue presentQueue;
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

    FramebufferSize getFramebufferSize();

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    void release();

public:
    GLFWwindow* window;
    vkb::Instance instance;
    VkSurfaceKHR renderSurface;
    vkb::PhysicalDevice gpu;
    vkb::Device device;
    vkb::Swapchain swapchain;
    std::vector<VkImageView> swapImageViews;
    VmaAllocator allocator;
    GPUQueues queues;
};
