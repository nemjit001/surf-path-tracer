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

#define RECURSIVE_IMPLEMENTATION    0   // Use a simple recursive path tracing implementation with no variance reduction & clamped depth
#define COLOR_BLACK                 RgbColor(0.0f, 0.0f, 0.0f)

AccumulatorState::AccumulatorState(U32 width, U32 height)
    :
    totalSamples(0),
    bufferSize(width * height),
    buffer(static_cast<RgbaColor*>(MALLOC64(bufferSize * sizeof(RgbaColor))))
{
    assert(buffer != nullptr);
    memset(buffer, 0, bufferSize * sizeof(RgbaColor));
}

AccumulatorState::~AccumulatorState()
{
    FREE64(buffer);
}

Renderer::Renderer(RenderContext renderContext, RendererConfig config, PixelBuffer resultBuffer, Camera& camera, Scene& scene)
    :
    m_context(std::move(renderContext)),
    m_descriptorPool(m_context.device),
    m_framebufferSize(m_context.getFramebufferSize()),
    m_config(config),
    m_camera(camera),
    m_scene(scene),
    m_resultBuffer(resultBuffer)
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
    const F32 invSamples = 1.0f / static_cast<F32>(m_accumulator.totalSamples + m_config.samplesPerFrame);

#pragma omp parallel for schedule(dynamic)
    for (I32 y = 0; y < static_cast<I32>(m_resultBuffer.height); y++)
    {
        for (I32 x = 0; x < static_cast<I32>(m_resultBuffer.width); x++)
        {
            SizeType pixelIndex = x + y * m_resultBuffer.width;
            U32 pixelSeed = initSeed(static_cast<U32>(pixelIndex + m_accumulator.totalSamples * 1799)); // Init with random very large value -> too small and randomization 'smears' screen

            for (SizeType sample = 0; sample < m_config.samplesPerFrame; sample++)
            {
                Ray primaryRay = m_camera.getPrimaryRay(
                    static_cast<F32>(x) + randomRange(pixelSeed, -0.5f, 0.5f),
                    static_cast<F32>(y) + randomRange(pixelSeed, -0.5f, 0.5f)
                );

                RgbaColor color = RgbaColor(trace(pixelSeed, primaryRay), 1.0f);
                m_accumulator.buffer[pixelIndex] += color;
            }

            RgbaColor outColor = m_accumulator.buffer[pixelIndex] * invSamples;
            m_resultBuffer.pixels[pixelIndex] = RgbaToU32(outColor);
        }
    }

    m_accumulator.totalSamples += m_config.samplesPerFrame;

    // Update instrumentation data
    m_frameInstrumentationData.energy = 0.0f;
    m_frameInstrumentationData.totalSamples = static_cast<U32>(m_accumulator.totalSamples);
    for (SizeType y = 0; y < m_resultBuffer.height; y++)
    {
        for (SizeType x = 0; x < m_resultBuffer.width; x++)
        {
            SizeType pixelIndex = x + y * m_resultBuffer.width;
            RgbaColor outColor = m_accumulator.buffer[pixelIndex] * invSamples;
            m_frameInstrumentationData.energy += outColor.r + outColor.g + outColor.b;
        }
    }

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

RgbColor Renderer::trace(U32& seed, Ray& ray, U32 depth)
{
#if RECURSIVE_IMPLEMENTATION == 1
    if (depth > m_config.maxBounces)
        return COLOR_BLACK;

    if (!m_scene.intersect(ray))
        return m_scene.sampleBackground(ray);

    const Instance& instance = m_scene.hitInstance(ray.metadata.instanceIndex);
    const Mesh* mesh = instance.bvh->mesh();
    const Material* material = instance.material;

    if (material->isLight())
    {
        return material->emittance();
    }

    Float3 normal = instance.normal(ray.metadata.primitiveIndex, ray.metadata.hitCoordinates);
    Float2 textureCoordinate = mesh->textureCoordinate(ray.metadata.primitiveIndex, ray.metadata.hitCoordinates);

    // Handle backface hits -> normal needs to be flipped if colinear with hit direction
    if (ray.direction.dot(normal) > 0.0f)
        normal *= -1.0f;

    Float3 mediumScale(1.0f);
    if (ray.inMedium)
        mediumScale = expf(material->absorption * -ray.depth);

    F32 r = randomF32(seed);
    if (r < material->reflectivity)
    {
        Float3 newDirection = reflect(ray.direction, normal);
        Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
        Ray newRay(newOrigin, newDirection);
        newRay.inMedium = ray.inMedium;
        return material->albedo * mediumScale * trace(seed, newRay, depth + 1);
    }
    else if (r < material->reflectivity + material->refractivity)
    {
        F32 n1 = ray.inMedium ? material->indexOfRefraction : 1.0f;
        F32 n2 = ray.inMedium ? 1.0f : material->indexOfRefraction;
        F32 iorRatio = n1 / n2;

        F32 cosI = -ray.direction.dot(normal);
        F32 cosTheta2 = 1.0f - iorRatio * iorRatio * (1.0f - cosI * cosI);
        F32 Fresnel = 1.0f;
        if (cosTheta2 > 0.0f)
        {
            F32 a = n1 - n2;
            F32 b = n1 + n2;
            F32 r0 = (a * a) / (b * b);
            F32 c = 1.0f - cosI;
            F32 Fresnel = r0 + (1.0f - r0) * (c * c * c * c * c);

            Float3 newDirection = iorRatio * ray.direction + ((iorRatio * cosI - sqrtf(fabsf(cosTheta2))) * normal);
            Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
            Ray newTransmit(newOrigin, newDirection);
            newTransmit.inMedium = !ray.inMedium;

            if (randomF32(seed) > Fresnel)
                return material->albedo * mediumScale * trace(seed, newTransmit, depth + 1);
        }

        Float3 newDirection = reflect(ray.direction, normal);
        Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
        Ray newReflect(newOrigin, newDirection);
        newReflect.inMedium = ray.inMedium;
        return material->albedo * mediumScale * trace(seed, newReflect, depth + 1);
    }
    else
    {
        RgbColor brdf = material->albedo * F32_INV_PI;

        Float3 newDirection = randomOnHemisphere(seed, normal);
        Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
        Ray newRay(newOrigin, newDirection);

        F32 cosTheta = newDirection.dot(normal);
        return material->emittance() + F32_2PI * cosTheta * brdf * mediumScale * trace(seed, newRay, depth + 1);
    }
#else
    // Non recursive path tracing implementation
    RgbColor energy(0.0f);
    RgbColor transmission(1.0f);
    for (;;)
    {
        if (!m_scene.intersect(ray))
        {
            energy += transmission * m_scene.sampleBackground(ray);
            break;
        }

        const Instance& instance = m_scene.hitInstance(ray.metadata.instanceIndex);
        const Mesh* mesh = instance.bvh->mesh();
        const Material* material = instance.material;

        if (material->isLight())
        {
            energy += transmission * material->emittance();
            break;
        }

        // Calculate termination chance & new scalefor russian roulette
        const F32 p = clamp(max(transmission.r, max(transmission.g, transmission.b)), 0.0f, 1.0f);
        if (p < randomF32(seed))
            break;

        Float3 normal = instance.normal(ray.metadata.primitiveIndex, ray.metadata.hitCoordinates);
        Float2 textureCoordinate = mesh->textureCoordinate(ray.metadata.primitiveIndex, ray.metadata.hitCoordinates);
        const F32 rrScale = 1.0f / p;

        // Handle backface hits -> normal needs to be flipped if colinear with hit direction
        if (ray.direction.dot(normal) > 0.0f)
            normal *= -1.0f;

        Float3 mediumScale(1.0f);
        if (ray.inMedium)
            mediumScale = expf(material->absorption * -ray.depth);

        F32 r = randomF32(seed);
        if (r < material->reflectivity)
        {
            Float3 newDirection = reflect(ray.direction, normal);
            Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
            bool wasInMedium = ray.inMedium;
            ray = Ray(newOrigin, newDirection);
            ray.inMedium = wasInMedium;

            transmission *= material->albedo * rrScale * mediumScale;
        }
        else if (r < (material->reflectivity + material->refractivity))
        {
            F32 n1 = ray.inMedium ? material->indexOfRefraction : 1.0f;
            F32 n2 = ray.inMedium ? 1.0f : material->indexOfRefraction;
            F32 iorRatio = n1 / n2;

            F32 cosI = -ray.direction.dot(normal);
            F32 cosTheta2 = 1.0f - iorRatio * iorRatio * (1.0f - cosI * cosI);
            F32 Fresnel = 1.0f;
            if (cosTheta2 > 0.0f)
            {
                F32 a = n1 - n2;
                F32 b = n1 + n2;
                F32 r0 = (a * a) / (b * b);
                F32 c = 1.0f - cosI;
                F32 Fresnel = r0 + (1.0f - r0) * (c * c * c * c * c);

                Float3 newDirection = iorRatio * ray.direction + ((iorRatio * cosI - sqrtf(fabsf(cosTheta2))) * normal);
                Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
                bool wasInMedium = ray.inMedium;
                ray = Ray(newOrigin, newDirection);
                ray.inMedium = !wasInMedium;

                if (randomF32(seed) > Fresnel)
                    transmission *= material->albedo * rrScale * mediumScale;
            }
            else
            {
                Float3 newDirection = reflect(ray.direction, normal);
                Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
                bool wasInMedium = ray.inMedium;
                ray = Ray(newOrigin, newDirection);
                ray.inMedium = wasInMedium;
                transmission *= material->albedo * rrScale * mediumScale;
            }
        }
        else
        {
            Float3 newDirection = randomOnHemisphereCosineWeighted(seed, normal);
            Float3 newOrigin = ray.hitPosition() + F32_EPSILON * newDirection;
            ray = Ray(newOrigin, newDirection);

            F32 cosTheta = newDirection.dot(normal);
            F32 invCosTheta = 1.0f / cosTheta;
            RgbColor brdf = material->albedo * F32_INV_PI;
            F32 inversePdf = F32_PI * invCosTheta;

            transmission *= material->emittance() + rrScale * inversePdf * cosTheta * brdf * mediumScale;
        }
    }

    return energy;
#endif
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

WaveFrontRenderer::WaveFrontRenderer(RenderContext renderContext, RendererConfig config, Camera& camera, Scene& scene)
    :
    m_context(std::move(renderContext)),
    m_config(config),
    m_camera(camera),
    m_scene(scene)
{
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
}

WaveFrontRenderer::~WaveFrontRenderer()
{
    vkDeviceWaitIdle(m_context.device);

    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkWaitForFences(m_context.device, 1, &m_frames[i].frameReady, VK_TRUE, UINT64_MAX));
        vkDestroyCommandPool(m_context.device, m_frames[i].pool, nullptr);
        vkDestroyFence(m_context.device, m_frames[i].frameReady, nullptr);
        vkDestroySemaphore(m_context.device, m_frames[i].swapImageAvailable, nullptr);
        vkDestroySemaphore(m_context.device, m_frames[i].renderingFinished, nullptr);
    }
}

void WaveFrontRenderer::clearAccumulator()
{
    // TODO: clear accumulator buffer
}

void WaveFrontRenderer::render(F32 deltaTime)
{
    // TODO: render frame using compute shader pipelines
}
