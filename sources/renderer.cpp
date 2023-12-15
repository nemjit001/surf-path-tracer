#include "renderer.h"

#include <cassert>
#include <cstdio>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include "surf.h"

Renderer::Renderer()
    :
    m_window(nullptr),
    m_instace(),
    m_renderSurface(VK_NULL_HANDLE),
    m_gpu(),
    m_device(),
    m_swapchain()
{
    //
}

void Renderer::init(GLFWwindow* window)
{
    assert(window != nullptr);
    m_window = window;

    std::vector<const char*> extensions = {
#ifndef _NDEBUG // Only enable debug reporting in Debug mode
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Initialize Vulkan instance
    vkb::InstanceBuilder instanceBuilder;
    vkb::Result<vkb::Instance> instanceResult = instanceBuilder
        .require_api_version(VK_API_VERSION_1_3)
        .set_app_name(PROGRAM_NAME)
        .set_app_version(PROGRAM_VERSION)
        .set_engine_name("NO ENGINE")
        .set_engine_version(0)
        .enable_extensions(extensions)
#ifndef _NDEBUG // Debug only config for instance
        .request_validation_layers(true)
        .set_debug_messenger_severity(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        )
        .set_debug_messenger_type(
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        )
        .set_debug_callback_user_data_pointer(nullptr)
        .set_debug_callback(Renderer::debugCallback)
#endif
        .build();

    if (!instanceResult.has_value())
    {
        // TODO: handle error
    }

    m_instace = instanceResult.value();

    // TODO: create surface using GLFW

    // Choose best GPU
    vkb::PhysicalDeviceSelector gpuSelector(m_instace);
    vkb::Result<vkb::PhysicalDevice> gpuResult = gpuSelector
        .set_surface(m_renderSurface)
        .set_minimum_version(1, 3)
        .select();

    if (!gpuResult.has_value())
    {
        // TODO: handle error
    }

    m_gpu = gpuResult.value();

    vkb::DeviceBuilder deviceBuilder(m_gpu);
    vkb::Result<vkb::Device> deviceResult = deviceBuilder.build();

    if (!deviceResult.has_value())
    {
        // TODO: handle error
    }

    m_device = deviceResult.value();
}

void Renderer::destroy()
{
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instace, nullptr);
}

void Renderer::render(float deltaTime)
{
    // Tick renderer
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    printf("[Vulkan] %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}
