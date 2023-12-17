#include "renderer.h"

// First use of VMA -> define implementation here
#define VMA_IMPLEMENTATION

#include <cassert>
#include <cstdio>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "surf.h"
#include "types.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/shader.h"
#include "vk_layer/vk_check.h"

Renderer::Renderer()
    :
    m_window(nullptr),
    m_instace(),
    m_renderSurface(VK_NULL_HANDLE),
    m_gpu(),
    m_device(),
    m_swapchain(),
    m_allocator(VK_NULL_HANDLE),
    m_transferQueue(VK_NULL_HANDLE),
    m_computeQueue(VK_NULL_HANDLE),
    m_graphicsQueue(VK_NULL_HANDLE),
    m_presentQueue(VK_NULL_HANDLE),
    m_currentFrame(0),
    m_frames{},
    m_presentPass(),
    m_swapImageViews(),
    m_framebuffers(),
    m_presentPipelineLayout(),
    m_presentPipeline()
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

    VK_CHECK(glfwCreateWindowSurface(m_instace, m_window, nullptr, &m_renderSurface));

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
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_clipped(false)
        .set_image_usage_flags(
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        )
        .build();

    m_swapchain = swapchainResult.value();
    m_swapImageViews = m_swapchain.get_image_views().value();

    // Create an allocator for Vulkan resources
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = 0;
    allocatorCreateInfo.instance = m_instace;
    allocatorCreateInfo.physicalDevice = m_gpu;
    allocatorCreateInfo.device = m_device;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));

    // Retrieve queue setup
    vkb::Result<VkQueue> transferQueue = m_device.get_queue(vkb::QueueType::transfer);
    vkb::Result<VkQueue> computeQueue = m_device.get_queue(vkb::QueueType::compute);
    vkb::Result<VkQueue> graphicsQueue = m_device.get_queue(vkb::QueueType::graphics);
    vkb::Result<VkQueue> presentQueue = m_device.get_queue(vkb::QueueType::present);

    if (
        transferQueue.matches_error(vkb::QueueError::transfer_unavailable)
        || computeQueue.matches_error(vkb::QueueError::compute_unavailable)    
    )
    {
        // If no dedicated transfer queue or compute queue is available, just use the available graphics queue
        // as its guaranteed to support transfer and compute operations
        transferQueue = m_device.get_queue(vkb::QueueType::graphics);
        computeQueue = m_device.get_queue(vkb::QueueType::graphics);
    }

    m_transferQueue = transferQueue.value();
    m_computeQueue = computeQueue.value();
    m_graphicsQueue = graphicsQueue.value();
    m_presentQueue = presentQueue.value();

    // Set up per frame structures
    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = m_device.get_queue_index(vkb::QueueType::graphics).value();

        VK_CHECK(vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_frames[i].pool));

        VkCommandBufferAllocateInfo bufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferAllocateInfo.commandPool = m_frames[i].pool;
        bufferAllocateInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(m_device, &bufferAllocateInfo, &m_frames[i].commandBuffer));
    }

    // Set up present pass
    m_presentPass.init(m_device, m_swapchain.image_format);

    // Create framebuffers for swapchain images
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    m_framebuffers.reserve(m_swapchain.image_count);
    for (const auto& imageView : m_swapImageViews)
    {
        Framebuffer swapFramebuffer;
        swapFramebuffer.init(m_device, m_presentPass, { imageView }, 1280, 720);
        m_framebuffers.push_back(swapFramebuffer);
    }

    // Set up pipeline layout & pipeline
    m_presentPipelineLayout.init(m_device);

    // Set up a viewport for the window size
    Viewport renderViewport = Viewport {
        0, 0,   // Offset of viewport in (X, Y)
        static_cast<U32>(width), static_cast<U32>(height),
        0.0f, 1.0f
    };

    // Instantiate shaders
    Shader presentVertShader;
    Shader presentFragShader;
    presentVertShader.initFromFile(m_device, ShaderType::Vertex, "shaders/fs_quad_vert.glsl.spv");
    presentFragShader.initFromFile(m_device, ShaderType::Fragment, "shaders/fs_quad_frag.glsl.spv");

    m_presentPipeline.init(
        m_device,
        renderViewport,
        m_presentPass,
        m_presentPipelineLayout,
        { presentVertShader, presentFragShader }
    );

    presentVertShader.destroy();
    presentFragShader.destroy();
}

void Renderer::destroy()
{
    m_presentPipeline.destroy();
    m_presentPipelineLayout.destroy();

    for (auto& framebuffer : m_framebuffers)
    {
        framebuffer.destroy();
    }

    m_presentPass.destroy();

    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyCommandPool(m_device, m_frames[i].pool, nullptr);
    }

    vmaDestroyAllocator(m_allocator);

    for (auto& view : m_swapImageViews)
    {
        vkDestroyImageView(m_device, view, nullptr);
    }

    vkb::destroy_swapchain(m_swapchain);
    vkb::destroy_device(m_device);
    vkb::destroy_surface(m_instace, m_renderSurface);
    vkb::destroy_instance(m_instace);
}

void Renderer::render(F32 deltaTime)
{
    U32 availableSwapImage = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &availableSwapImage);
    if (acquireResult != VK_SUCCESS)
    {
        if (acquireResult == VK_SUBOPTIMAL_KHR || acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // recreate swapchain
            return; // cannot submit work to newly created swapchain yet -> wait for next frame
        }
        else
        {
            // Abort execution
        }
    }

    VkSubmitInfo presentPassSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    presentPassSubmit.commandBufferCount = 0;
    presentPassSubmit.pCommandBuffers = nullptr;
    presentPassSubmit.waitSemaphoreCount = 0;
    presentPassSubmit.pWaitSemaphores = nullptr;
    presentPassSubmit.pWaitDstStageMask = nullptr;
    presentPassSubmit.signalSemaphoreCount = 0;
    presentPassSubmit.pSignalSemaphores = nullptr;

    VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &presentPassSubmit, VK_NULL_HANDLE));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain.swapchain;
    presentInfo.pImageIndices = &availableSwapImage;
    presentInfo.pResults = nullptr;
    presentInfo.waitSemaphoreCount = 0;
    presentInfo.pWaitSemaphores = nullptr;

    VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (presentResult != VK_SUCCESS)
    {
        if (presentResult == VK_SUBOPTIMAL_KHR || presentResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // recreate swapchain
        }
        else
        {
            // Abort execution
        }
    }
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
