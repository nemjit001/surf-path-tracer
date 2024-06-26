#include "render_context.h"

// First use of VMA -> define implementation here
#define VMA_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "surf.h"
#include "vk_layer/vk_check.h"

RenderContext::RenderContext(GLFWwindow* window)
	:
	window(window),
	instance(),
	renderSurface(VK_NULL_HANDLE),
	gpu(),
	device(),
	swapchain(),
	swapImageViews(),
	allocator(VK_NULL_HANDLE),
    queues{}
{
    assert(window != nullptr);
    window = window;

    // Set up Vulkan extensions
    std::vector<const char*> extensions = {
#ifndef NDEBUG // Only enable debug reporting in Debug mode
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    // Set up device features
    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkPhysicalDeviceVulkan13Features deviceVk13Features = {};
    deviceVk13Features.robustImageAccess = true;
    deviceVk13Features.computeFullSubgroups = true;
    deviceVk13Features.synchronization2 = true;

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
#ifdef SURF_DEBUG_REPORT // Debug only config for instance
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
        .set_debug_callback(RenderContext::debugCallback)
#endif
        .build();

    instance = instanceResult.value();

    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &renderSurface));

    // Choose a GPU that supports the required features
    vkb::PhysicalDeviceSelector gpuSelector(instance);
    vkb::Result<vkb::PhysicalDevice> gpuResult = gpuSelector
        .set_surface(renderSurface)
        .set_minimum_version(1, 3)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .require_present(true)
        .add_required_extensions({
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
            })
        .set_required_features(deviceFeatures)
        .set_required_features_13(deviceVk13Features)
        .select();

    gpu = gpuResult.value();

    // Create a logical device based on the chosen GPU
    vkb::DeviceBuilder deviceBuilder(gpu);
    vkb::Result<vkb::Device> deviceResult = deviceBuilder
        .build();

    device = deviceResult.value();

    // Create a swapchain
    vkb::SwapchainBuilder swapchainBuilder(device);
    vkb::Result<vkb::Swapchain> swapchainResult = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_required_min_image_count(vkb::SwapchainBuilder::BufferMode::TRIPLE_BUFFERING)
        .set_clipped(false)
        .set_image_usage_flags(
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        )
        .build();

    swapchain = swapchainResult.value();
    swapImageViews = swapchain.get_image_views().value();

    // Create an allocator for Vulkan resources
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = 0;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.physicalDevice = gpu;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &allocator));

    // Retrieve queue setup
    vkb::Result<VkQueue> transferQueue = device.get_queue(vkb::QueueType::transfer);
    vkb::Result<U32> transferQueueIndex = device.get_queue_index(vkb::QueueType::transfer);

    vkb::Result<VkQueue> computeQueue = device.get_queue(vkb::QueueType::compute);
    vkb::Result<U32> computeQueueIndex = device.get_queue_index(vkb::QueueType::compute);

    vkb::Result<VkQueue> graphicsQueue = device.get_queue(vkb::QueueType::graphics);
    vkb::Result<U32> graphicsQueueIndex = device.get_queue_index(vkb::QueueType::graphics);

    vkb::Result<VkQueue> presentQueue = device.get_queue(vkb::QueueType::present);
    vkb::Result<U32> presentQueueIndex = device.get_queue_index(vkb::QueueType::present);

    if (
        transferQueue.matches_error(vkb::QueueError::transfer_unavailable)
        || computeQueue.matches_error(vkb::QueueError::compute_unavailable)
        )
    {
        // If no dedicated transfer queue or compute queue is available, just use the available graphics queue
        // as its guaranteed to support transfer and compute operations
        transferQueue = graphicsQueue;
        transferQueueIndex = graphicsQueueIndex;
        computeQueue = graphicsQueue;
        computeQueueIndex = graphicsQueueIndex;
    }

    queues.transferQueue = GPUQueue{ transferQueueIndex.value(), transferQueue.value() };
    queues.computeQueue = GPUQueue{ computeQueueIndex.value(), computeQueue.value() };
    queues.graphicsQueue = GPUQueue{ graphicsQueueIndex.value(), graphicsQueue.value() };
    queues.presentQueue = GPUQueue{ presentQueueIndex.value(), presentQueue.value() };
}

RenderContext::~RenderContext()
{
    release();
}

RenderContext::RenderContext(RenderContext&& other) noexcept
    :
    window(other.window),
    instance(other.instance),
    renderSurface(other.renderSurface),
    gpu(other.gpu),
    device(other.device),
    swapchain(other.swapchain),
    swapImageViews(other.swapImageViews),
    allocator(other.allocator),
    queues(other.queues)
{
    other.instance = vkb::Instance();
    other.renderSurface = VK_NULL_HANDLE;
    other.gpu = vkb::PhysicalDevice();
    other.device = vkb::Device();
    other.swapchain = vkb::Swapchain();
    other.swapImageViews.clear();
    other.allocator = VK_NULL_HANDLE;
    other.queues = GPUQueues {};
}

RenderContext& RenderContext::operator=(RenderContext&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    window = other.window;
    instance = other.instance;
    renderSurface = other.renderSurface;
    gpu = other.gpu;
    device = other.device;
    swapchain = other.swapchain;
    swapImageViews = other.swapImageViews;
    allocator = other.allocator;
    queues = other.queues;

    return *this;
}

FramebufferSize RenderContext::getFramebufferSize()
{
    int glfwWidth = 0, glfwHeight = 0;
    glfwGetFramebufferSize(window, &glfwWidth, &glfwHeight);

    return FramebufferSize{
        static_cast<U32>(glfwWidth),
        static_cast<U32>(glfwHeight)
    };
}

VKAPI_ATTR VkBool32 VKAPI_CALL RenderContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    printf("[Vulkan] %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

void RenderContext::release()
{
    if (instance.instance == VK_NULL_HANDLE)
    {
        // Resources have been released by move
        return;
    }

    vmaDestroyAllocator(allocator);

    for (auto& view : swapImageViews)
    {
        vkDestroyImageView(device, view, nullptr);
    }

    vkb::destroy_swapchain(swapchain);
    vkb::destroy_device(device);
    vkb::destroy_surface(instance, renderSurface);
    vkb::destroy_instance(instance);
}
