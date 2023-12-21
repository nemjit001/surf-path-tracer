#include "renderer.h"

#include <cassert>
#include <cstdio>
#include <vulkan/vulkan.h>

#include "camera.h"
#include "ray.h"
#include "render_context.h"
#include "scene.h"
#include "surf_math.h"
#include "types.h"
#include "pixel_buffer.h"
#include "vk_layer/buffer.h"
#include "vk_layer/descriptor_pool.h"
#include "vk_layer/framebuffer.h"
#include "vk_layer/image.h"
#include "vk_layer/pipeline.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/sampler.h"
#include "vk_layer/vk_check.h"

// TODO: expose as settings for renderer
#define MAX_BOUNCES         5
#define SAMPLES_PER_FRAME   1

AccumulatorState::AccumulatorState(U32 width, U32 height)
    :
    totalSamples(0),
    bufferSize(width * height),
    buffer(new RgbaColor[bufferSize]{})
{
    //
}

AccumulatorState::~AccumulatorState()
{
    delete[] buffer;
}

Renderer::Renderer(RenderContext renderContext, PixelBuffer resultBuffer, Camera* camera, Scene* scene)
    :
    m_context(std::move(renderContext)),
    m_descriptorPool(m_context.device),
    m_framebufferSize(m_context.getFramebufferSize()),
    m_resultBuffer(resultBuffer),
    m_accumulator(resultBuffer.width, resultBuffer.height),
    m_camera(camera),
    m_scene(scene),
    m_copyFinishedFence(VK_NULL_HANDLE),
    m_copyPool(VK_NULL_HANDLE),
    m_oneshotCopyBuffer(VK_NULL_HANDLE),
    m_currentFrame(0),
    m_frames{},
    m_presentPass(m_context.device, m_context.swapchain.image_format),
    m_framebuffers(),
    m_frameStagingBuffer(
        m_context.allocator,
        resultBuffer.width * resultBuffer.height * sizeof(U32),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,            // Used as staging bufer for GPU uploads
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT       // Needs to be visible from the CPU
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,   // And always coherent with CPU memory during uploads
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT // Should allow memcpy writes & mapping
    ),
    m_frameImage(
        m_context.device,
        m_context.allocator,
        VkFormat::VK_FORMAT_R8G8B8A8_SRGB,                      // Standard RGBA format
        resultBuffer.width, resultBuffer.height,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT   // Used as transfer destination for CPU staging buffer
        | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT      // Used in present shader as sampled screen texture
    ),
    m_frameImageSampler(m_context.device),
    m_presentPipelineLayout(
        m_context.device,
        {DescriptorSetLayout{{
            DescriptorSetBinding{
                0,                                          // Binding
                VK_SHADER_STAGE_FRAGMENT_BIT,               // Shader stage
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER   // Descriptor Type
            }
        }}}
    ),
    m_presentPipeline()
{
    // Set up oneshot command pool, fence & copy buffer
    VkFenceCreateInfo copyFinishedCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    copyFinishedCreateInfo.flags = 0;
    VK_CHECK(vkCreateFence(m_context.device, &copyFinishedCreateInfo, nullptr, &m_copyFinishedFence));

    VkCommandPoolCreateInfo copyPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    copyPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    copyPoolInfo.queueFamilyIndex = m_context.queues.graphicsQueue.familyIndex;
    VK_CHECK(vkCreateCommandPool(m_context.device, &copyPoolInfo, nullptr, &m_copyPool));

    VkCommandBufferAllocateInfo oneshotCopyAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    oneshotCopyAllocateInfo.commandPool = m_copyPool;
    oneshotCopyAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    oneshotCopyAllocateInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(m_context.device, &oneshotCopyAllocateInfo, &m_oneshotCopyBuffer));

    // Set up per frame structures
    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = m_context.queues.graphicsQueue.familyIndex;

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
        Framebuffer swapFramebuffer(m_context.device, m_presentPass, { imageView }, m_framebufferSize.width, m_framebufferSize.height);
        m_framebuffers.push_back(std::move(swapFramebuffer));
    }

    // Instantiate shaders & pipeline
    Shader presentVertShader(m_context.device, ShaderType::Vertex, "shaders/fs_quad_vert.glsl.spv");
    Shader presentFragShader(m_context.device, ShaderType::Fragment, "shaders/fs_quad_frag.glsl.spv");

    m_presentPipeline.init(
        m_context.device,
        Viewport {
            0, 0,   // Offset of viewport in (X, Y)
            m_framebufferSize.width, m_framebufferSize.height,
            0.0f, 1.0f
        },
        m_descriptorPool,
        m_presentPass,
        m_presentPipelineLayout,
        { &presentVertShader, &presentFragShader }
    );

    m_presentPipeline.updateDescriptorSets({
        WriteDescriptorSet{
            0, 0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VkDescriptorImageInfo{
                m_frameImageSampler.handle(),
                m_frameImage.view(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        }
    });
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

    vkDestroyCommandPool(m_context.device, m_copyPool, nullptr);
    vkDestroyFence(m_context.device, m_copyFinishedFence, nullptr);
}

RgbColor Renderer::trace(Ray& ray, U32 depth)
{
    if (depth > MAX_BOUNCES)
        return RgbColor(0.0f, 0.0f, 0.0f);

    if (!m_scene->intersect(ray))
        return RgbColor(0.0f, 0.0f, 0.0f);

    // TODO: Fetch material (emittance, albedo, specularity, reflectivity)
    Float3 hitNormal = m_scene->normal(ray.metadata.instanceIndex, ray.metadata.primitiveIndex);
    RgbColor emittance = 0.5f * (RgbColor(1.0f) + hitNormal);   // For now set color as tri normal
    RgbColor albedo = RgbColor(0.0f, 0.0f, 0.0f);               // TODO: Retrieve from hit instance material

    if (ray.metadata.primitiveIndex % 2 == 0)
    {
        albedo = emittance;
        emittance = Float3(0.0f);
    }

    // Simple isLight check for emittance
    // TODO: replace with material.isLight()
    if (emittance.r > 0.0f || emittance.g > 0.0f || emittance.b > 0.0f)
    {
        return emittance;
    }
    
    RgbColor brdf = albedo * F32_INV_PI;

    Float3 newDirection = randomOnHemisphere(hitNormal);
    Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
    Ray newRay(newOrigin, newDirection);

    F32 cosTheta = glm::dot(newDirection, hitNormal);
    RgbColor incomingColor = trace(newRay, depth + 1);

    return emittance + F32_2PI * cosTheta * brdf * incomingColor;
}

void Renderer::clearAccumulator()
{
    m_accumulator.totalSamples = 0;
    memset(m_accumulator.buffer, 0, m_accumulator.bufferSize * sizeof(RgbaColor));
}

void Renderer::render(F32 deltaTime)
{
    const FrameData& activeFrame = m_frames[m_currentFrame];

    U32 availableSwapImage = 0;
    VK_CHECK(vkAcquireNextImageKHR(m_context.device, m_context.swapchain, UINT64_MAX, activeFrame.swapImageAvailable, VK_NULL_HANDLE, &availableSwapImage));

    VK_CHECK(vkWaitForFences(m_context.device, 1, &activeFrame.frameReady, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context.device, 1, &activeFrame.frameReady));
    VK_CHECK(vkResetCommandBuffer(activeFrame.commandBuffer, /* Empty reset flags */ 0));

    // Start CPU ray tracing loop
    const F32 invSamples = 1.0f / static_cast<F32>(m_accumulator.totalSamples + SAMPLES_PER_FRAME);

#pragma omp parallel for schedule(dynamic)
    for (I32 y = 0; y < static_cast<I32>(m_resultBuffer.height); y++)
    {
        for (I32 x = 0; x < static_cast<I32>(m_resultBuffer.width); x++)
        {
            SizeType pixelIndex = x + y * m_resultBuffer.width;

            for (SizeType sample = 0; sample < SAMPLES_PER_FRAME; sample++)
            {
                Ray primaryRay = m_camera->getPrimaryRay(static_cast<F32>(x), static_cast<F32>(y));
                RgbaColor color = RgbaColor(trace(primaryRay), 1.0f);

                m_accumulator.buffer[pixelIndex] += color;
            }

            m_resultBuffer.pixels[pixelIndex] = RgbaToU32(m_accumulator.buffer[pixelIndex] * invSamples);
        }
    }

    m_accumulator.totalSamples += SAMPLES_PER_FRAME;
    // End CPU ray tracing loop

    m_frameStagingBuffer.copyToBuffer(m_resultBuffer.width * m_resultBuffer.height * sizeof(U32), m_resultBuffer.pixels);
    copyBufferToImage(m_frameStagingBuffer, m_frameImage);

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

    VK_CHECK(vkQueueSubmit(m_context.queues.graphicsQueue.handle, 1, &presentPassSubmit, activeFrame.frameReady));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_context.swapchain.swapchain;
    presentInfo.pImageIndices = &availableSwapImage;
    presentInfo.pResults = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &activeFrame.renderingFinished;

    VK_CHECK(vkQueuePresentKHR(m_context.queues.presentQueue.handle, &presentInfo));
    m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
}

void Renderer::copyBufferToImage(
    const Buffer& staging,
    const Image& target
)
{
    VkCommandBufferBeginInfo oneshotBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    oneshotBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    oneshotBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(m_oneshotCopyBuffer, &oneshotBeginInfo));

    VkImageMemoryBarrier transferTransitionBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    transferTransitionBarrier.srcAccessMask = 0;
    transferTransitionBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    transferTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transferTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transferTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferTransitionBarrier.image = target.handle();
    transferTransitionBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,
        0, 1
    };

    vkCmdPipelineBarrier(
        m_oneshotCopyBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &transferTransitionBarrier
    );

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageOffset = { 0, 0, 0 };
    copyRegion.imageExtent = { m_resultBuffer.width, m_resultBuffer.height, 1 };
    copyRegion.imageSubresource = VkImageSubresourceLayers{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,  // 0th mip level
        0, 1
    };

    vkCmdCopyBufferToImage(
        m_oneshotCopyBuffer,
        staging.handle(),
        target.handle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
    );

    VkImageMemoryBarrier shaderTransitionBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    shaderTransitionBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    shaderTransitionBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    shaderTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    shaderTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shaderTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shaderTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shaderTransitionBarrier.image = target.handle();
    shaderTransitionBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,
        0, 1
    };

    vkCmdPipelineBarrier(
        m_oneshotCopyBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &shaderTransitionBarrier
    );

    VK_CHECK(vkEndCommandBuffer(m_oneshotCopyBuffer));

    VkSubmitInfo oneshotSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    oneshotSubmit.commandBufferCount = 1;
    oneshotSubmit.pCommandBuffers = &m_oneshotCopyBuffer;
    oneshotSubmit.waitSemaphoreCount = 0;
    oneshotSubmit.pWaitSemaphores = nullptr;
    oneshotSubmit.pWaitDstStageMask = nullptr;
    oneshotSubmit.signalSemaphoreCount = 0;
    oneshotSubmit.pSignalSemaphores = nullptr;
    
    VK_CHECK(vkQueueSubmit(m_context.queues.graphicsQueue.handle, 1, &oneshotSubmit, m_copyFinishedFence));
    VK_CHECK(vkWaitForFences(m_context.device, 1, &m_copyFinishedFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context.device, 1, &m_copyFinishedFence));
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
    renderPassBeginInfo.renderArea = {
        0, 0,
        m_framebufferSize.width, m_framebufferSize.height
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    const std::vector<VkDescriptorSet>& descriptorSets = m_presentPipeline.descriptorSets();
    vkCmdBindDescriptorSets(
        commandBuffer,
        m_presentPipeline.bindPoint(),
        m_presentPipelineLayout.handle(),
        0, static_cast<U32>(descriptorSets.size()),
        descriptorSets.data(),
        0, nullptr
    );

    vkCmdBindPipeline(commandBuffer, m_presentPipeline.bindPoint(), m_presentPipeline.handle());
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);   // Single instance -> shader takes vertex index and transforms it to screen coords

    vkCmdEndRenderPass(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}
