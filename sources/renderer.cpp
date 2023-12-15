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

    // Set up Vulkan extensions
    std::vector<const char*> extensions = {
#ifndef NDEBUG // Only enable debug reporting in Debug mode
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
#ifndef NDEBUG // Debug only config for instance
        .request_validation_layers(true)
        .set_debug_messenger_severity(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
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

    m_instace = instanceResult.value();

    // TODO: create surface using GLFW
    if (glfwCreateWindowSurface(m_instace, m_window, nullptr, &m_renderSurface) != VK_SUCCESS)
    {
        // TODO: handle error
    }

    // Choose a GPU that supports the required features
    vkb::PhysicalDeviceSelector gpuSelector(m_instace);
    vkb::Result<vkb::PhysicalDevice> gpuResult = gpuSelector
        .set_surface(m_renderSurface)
        .set_minimum_version(1, 3)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .require_present(true)
        .add_required_extensions({
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        })
        .select();

    m_gpu = gpuResult.value();

    // Create a logical device based on the chosen GPU
    vkb::DeviceBuilder deviceBuilder(m_gpu);
    vkb::Result<vkb::Device> deviceResult = deviceBuilder
        .build();

    m_device = deviceResult.value();

    // Create a swapchain
    // TODO: Choose good format, image count, present mode etc.
    vkb::SwapchainBuilder swapchainBuilder(m_device);
    vkb::Result<vkb::Swapchain> swapchainResult = swapchainBuilder
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_clipped(false)
        .set_image_usage_flags(
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        )
        .build();

    m_swapchain = swapchainResult.value();
}

void Renderer::destroy()
{
    vkb::destroy_swapchain(m_swapchain);
    vkb::destroy_device(m_device);
    vkb::destroy_surface(m_instace, m_renderSurface);
    vkb::destroy_instance(m_instace);
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
