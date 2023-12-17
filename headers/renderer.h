#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/framebuffer.h"
#include "vk_layer/pipeline.h"
#include "vk_layer/render_pass.h"

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
    // Window pointer
    GLFWwindow* m_window;

    // Render context
    vkb::Instance m_instace;
    VkSurfaceKHR m_renderSurface;
    vkb::PhysicalDevice m_gpu;
    vkb::Device m_device;
    vkb::Swapchain m_swapchain;
    VmaAllocator m_allocator;
    VkQueue m_transferQueue;
    VkQueue m_computeQueue;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    // Default render pass w/ framebuffers
    RenderPass m_presentPass;
    std::vector<VkImageView> m_swapImageViews;
    std::vector<Framebuffer> m_framebuffers;

    // Custom shader pipeline & layout for use during render pass
    PipelineLayout m_presentPipelineLayout;
    GraphicsPipeline m_presentPipeline;
};
