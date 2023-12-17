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

Renderer::Renderer(RenderContext renderContext, RenderResulution resolution)
    :
    m_context(std::move(renderContext)),
    m_currentFrame(0),
    m_frames{},
    m_presentPass(m_context.device, m_context.swapchain.image_format),
    m_framebuffers(),
    m_presentPipelineLayout(m_context.device),
    m_presentPipeline()
{
    // Set up per frame structures
    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = m_context.device.get_queue_index(vkb::QueueType::graphics).value();

        VK_CHECK(vkCreateCommandPool(m_context.device, &poolCreateInfo, nullptr, &m_frames[i].pool));

        VkCommandBufferAllocateInfo bufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferAllocateInfo.commandPool = m_frames[i].pool;
        bufferAllocateInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(m_context.device, &bufferAllocateInfo, &m_frames[i].commandBuffer));

        VkFenceCreateInfo frameReadyCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        frameReadyCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(m_context.device, &frameReadyCreateInfo, nullptr, &m_frames[i].frameReady));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(m_context.device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapImageAvailable));
        VK_CHECK(vkCreateSemaphore(m_context.device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderingFinished));
    }

    // Create framebuffers for swapchain images
    m_framebuffers.reserve(m_context.swapchain.image_count);
    for (const auto& imageView : m_context.swapImageViews)
    {
        Framebuffer swapFramebuffer(m_context.device, m_presentPass, { imageView }, resolution.width, resolution.height);
        m_framebuffers.push_back(std::move(swapFramebuffer));
    }

    // Instantiate shaders & pipeline
    Shader presentVertShader(m_context.device, ShaderType::Vertex, "shaders/fs_quad_vert.glsl.spv");
    Shader presentFragShader(m_context.device, ShaderType::Fragment, "shaders/fs_quad_frag.glsl.spv");

    m_presentPipeline.init(
        m_context.device,
        Viewport {
            0, 0,   // Offset of viewport in (X, Y)
            resolution.width, resolution.height,
            0.0f, 1.0f
        },
        m_presentPass,
        m_presentPipelineLayout,
        { &presentVertShader, &presentFragShader }
    );
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(m_context.device);

    m_presentPipeline.destroy();

    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkWaitForFences(m_context.device, 1, &m_frames[i].frameReady, VK_TRUE, UINT64_MAX));
        vkDestroyCommandPool(m_context.device, m_frames[i].pool, nullptr);
        vkDestroyFence(m_context.device, m_frames[i].frameReady, nullptr);
        vkDestroySemaphore(m_context.device, m_frames[i].swapImageAvailable, nullptr);
        vkDestroySemaphore(m_context.device, m_frames[i].renderingFinished, nullptr);
    }
}

void Renderer::recordFrame(
    VkCommandBuffer commandBuffer,
    const Framebuffer& framebuffer,
    const RenderPass& renderPass
)
{
    VkCommandBufferBeginInfo bufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bufferBeginInfo.flags = 0;
    bufferBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo));

    VkClearValue clearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };

    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassBeginInfo.renderPass = renderPass.handle();
    renderPassBeginInfo.framebuffer = framebuffer.handle();
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;
    renderPassBeginInfo.renderArea = {  // FIXME: take from some value stored in renderer
        0, 0,
        1280, 720
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Draw fullscreen quad
    vkCmdBindPipeline(commandBuffer, m_presentPipeline.bindPoint(), m_presentPipeline.handle());
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);   // Single instance -> shader takes vertex index and transforms it to screen coords

    vkCmdEndRenderPass(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void Renderer::render(F32 deltaTime)
{
    const FrameData& activeFrame = m_frames[m_currentFrame];

    U32 availableSwapImage = 0;
    VK_CHECK(vkAcquireNextImageKHR(m_context.device, m_context.swapchain, UINT64_MAX, activeFrame.swapImageAvailable, VK_NULL_HANDLE, &availableSwapImage));

    VK_CHECK(vkWaitForFences(m_context.device, 1, &activeFrame.frameReady, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context.device, 1, &activeFrame.frameReady));
    VK_CHECK(vkResetCommandBuffer(activeFrame.commandBuffer, /* Empty rest flags */ 0));

    // TODO: For pixel in screen buffer (CPU RAM buffer) do Kajiya Path Tracing -> massively parallel w/ openMP?

    // TODO: copy rendered frame to staging buffer + upload staging buffer to image

    const Framebuffer& activeFramebuffer = m_framebuffers[availableSwapImage];
    recordFrame(activeFrame.commandBuffer, activeFramebuffer, m_presentPass);

    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT   // Wait until color output has been written to signal finish
    };

    VkSubmitInfo presentPassSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    presentPassSubmit.commandBufferCount = 1;
    presentPassSubmit.pCommandBuffers = &activeFrame.commandBuffer;
    presentPassSubmit.waitSemaphoreCount = 1;
    presentPassSubmit.pWaitSemaphores = &activeFrame.swapImageAvailable;
    presentPassSubmit.pWaitDstStageMask = waitStages;
    presentPassSubmit.signalSemaphoreCount = 1;
    presentPassSubmit.pSignalSemaphores = &activeFrame.renderingFinished;

    VK_CHECK(vkQueueSubmit(m_context.queues.graphicsQueue, 1, &presentPassSubmit, activeFrame.frameReady));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_context.swapchain.swapchain;
    presentInfo.pImageIndices = &availableSwapImage;
    presentInfo.pResults = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &activeFrame.renderingFinished;

    VK_CHECK(vkQueuePresentKHR(m_context.queues.presentQueue, &presentInfo));
    m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
}
