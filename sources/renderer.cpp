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

// Threshold for difference in ray counts between waves in wavefront path tracing
#define WF_RAY_DIFF_THRESHOLD           50
// Batch size that is allowed to be deferred to the next frame in wavefront path tracing
#define WF_RAY_NF_BATCH_SIZE            500
// Output lumen data WARN: drops framerate to sub second on discrete GPUs
#define WF_LUMEN_OUTPUT                 0

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

Renderer::Renderer(RenderContext* renderContext, UIManager* uiManager, RendererConfig config, PixelBuffer resultBuffer, Camera& camera, Scene& scene)
    :
    m_context(renderContext),
    m_uiManager(uiManager),
    m_descriptorPool(m_context->device),
    m_framebufferSize(m_context->getFramebufferSize()),
    m_config(config),
    m_camera(camera),
    m_scene(scene),
    m_resultBuffer(resultBuffer)
{
    // Set up oneshot command pool, fence & copy buffer
    VkFenceCreateInfo copyFinishedCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    copyFinishedCreateInfo.flags = 0;
    VK_CHECK(vkCreateFence(m_context->device, &copyFinishedCreateInfo, nullptr, &m_copyFinishedFence));

    VkCommandPoolCreateInfo copyPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    copyPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    copyPoolInfo.queueFamilyIndex = m_context->queues.graphicsQueue.familyIndex;
    VK_CHECK(vkCreateCommandPool(m_context->device, &copyPoolInfo, nullptr, &m_copyPool));

    VkCommandBufferAllocateInfo oneshotCopyAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    oneshotCopyAllocateInfo.commandPool = m_copyPool;
    oneshotCopyAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    oneshotCopyAllocateInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(m_context->device, &oneshotCopyAllocateInfo, &m_oneshotCopyBuffer));

    // Set up per frame structures
    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = m_context->queues.graphicsQueue.familyIndex;

        VK_CHECK(vkCreateCommandPool(m_context->device, &poolCreateInfo, nullptr, &m_frames[i].pool));

        VkCommandBufferAllocateInfo bufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferAllocateInfo.commandPool = m_frames[i].pool;
        bufferAllocateInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_context->device, &bufferAllocateInfo, &m_frames[i].presentCommandBuffer));
        VK_CHECK(vkAllocateCommandBuffers(m_context->device, &bufferAllocateInfo, &m_frames[i].uiCommandBuffer));

        VkFenceCreateInfo frameReadyCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        frameReadyCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(m_context->device, &frameReadyCreateInfo, nullptr, &m_frames[i].frameReady));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapImageAvailable));
        VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderingFinished));
        VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_frames[i].uiPassFinished));
    }

    // Create framebuffers for swapchain images
    m_framebuffers.reserve(m_context->swapchain.image_count);
    for (const auto& imageView : m_context->swapImageViews)
    {
        Framebuffer swapFramebuffer(m_context->device, m_presentPass, { imageView }, m_framebufferSize.width, m_framebufferSize.height);
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
    vkDeviceWaitIdle(m_context->device);

    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkWaitForFences(m_context->device, 1, &m_frames[i].frameReady, VK_TRUE, UINT64_MAX));
        vkDestroyCommandPool(m_context->device, m_frames[i].pool, nullptr);
        vkDestroyFence(m_context->device, m_frames[i].frameReady, nullptr);
        vkDestroySemaphore(m_context->device, m_frames[i].swapImageAvailable, nullptr);
        vkDestroySemaphore(m_context->device, m_frames[i].renderingFinished, nullptr);
        vkDestroySemaphore(m_context->device, m_frames[i].uiPassFinished, nullptr);
    }

    vkDestroyCommandPool(m_context->device, m_copyPool, nullptr);
    vkDestroyFence(m_context->device, m_copyFinishedFence, nullptr);
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
    VK_CHECK(vkAcquireNextImageKHR(m_context->device, m_context->swapchain, UINT64_MAX, activeFrame.swapImageAvailable, VK_NULL_HANDLE, &availableSwapImage));

    VK_CHECK(vkWaitForFences(m_context->device, 1, &activeFrame.frameReady, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context->device, 1, &activeFrame.frameReady));
    VK_CHECK(vkResetCommandBuffer(activeFrame.uiCommandBuffer, /* Empty reset flags */ 0));
    VK_CHECK(vkResetCommandBuffer(activeFrame.presentCommandBuffer, /* Empty reset flags */ 0));

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
                    pixelSeed,
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

    // Record UI & present passes
    const Framebuffer& activeFramebuffer = m_framebuffers[availableSwapImage];
    recordFrame(activeFrame.presentCommandBuffer, activeFramebuffer, m_presentPass);
    m_uiManager->recordGUIPass(activeFrame.uiCommandBuffer, availableSwapImage);

    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT   // Wait until color output has been written to signal finish
    };

    VkSubmitInfo presentPassSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    presentPassSubmit.commandBufferCount = 1;
    presentPassSubmit.pCommandBuffers = &activeFrame.presentCommandBuffer;
    presentPassSubmit.waitSemaphoreCount = 1;
    presentPassSubmit.pWaitSemaphores = &activeFrame.swapImageAvailable;
    presentPassSubmit.pWaitDstStageMask = waitStages;
    presentPassSubmit.signalSemaphoreCount = 1;
    presentPassSubmit.pSignalSemaphores = &activeFrame.renderingFinished;

    VkSubmitInfo uiPassSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    uiPassSubmit.commandBufferCount = 1;
    uiPassSubmit.pCommandBuffers = &activeFrame.uiCommandBuffer;
    uiPassSubmit.waitSemaphoreCount = 1;
    uiPassSubmit.pWaitSemaphores = &activeFrame.renderingFinished;
    uiPassSubmit.pWaitDstStageMask = waitStages;
    uiPassSubmit.signalSemaphoreCount = 1;
    uiPassSubmit.pSignalSemaphores = &activeFrame.uiPassFinished;

    VkSubmitInfo renderPassSubmits[] = { presentPassSubmit, uiPassSubmit };
    VK_CHECK(vkQueueSubmit(m_context->queues.graphicsQueue.handle, 2, renderPassSubmits, activeFrame.frameReady));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_context->swapchain.swapchain;
    presentInfo.pImageIndices = &availableSwapImage;
    presentInfo.pResults = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &activeFrame.uiPassFinished;

    VK_CHECK(vkQueuePresentKHR(m_context->queues.presentQueue.handle, &presentInfo));
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
    bool lastSpecular = true;
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
            energy += lastSpecular ? transmission * material->emittance() : COLOR_BLACK;
            break;
        }

        Float3 mediumScale(1.0f);
        if (ray.inMedium)
            mediumScale = expf(material->absorption * -ray.depth);

        // Calculate termination chance for russian roulette
        const F32 p = clamp(max(transmission.r, max(transmission.g, transmission.b)), 0.0f, 1.0f);
        if (p < randomF32(seed))
            break;

        Float3 I = ray.hitPosition();
        Float3 N = instance.normal(ray.metadata.primitiveIndex, ray.metadata.hitCoordinates);
        Float2 UV = mesh->textureCoordinate(ray.metadata.primitiveIndex, ray.metadata.hitCoordinates);
        U32 lightCount = m_scene.lightCount();
        F32 rrScale = 1.0f / p;
        F32 rng = randomF32(seed);

        Float3 R = Float3(0);
        bool inMedium = ray.inMedium;

        // Flip normal on backface hits
        if (ray.direction.dot(N) > 0.0f)
            N *= -1.0f;

        if (rng < material->reflectance)
        {
            R = reflect(ray.direction, N);
            transmission *= material->baseColor * mediumScale * rrScale;
        }
        else if (rng < (material->reflectance + material->refractance))
        {
            // Assume reflect & precalculate ray
            bool mustRefract = false;
            R = reflect(ray.direction, N);

            // Check for refraction
            F32 n1 = ray.inMedium ? material->indexOfRefraction : 1.0f;
            F32 n2 = ray.inMedium ? 1.0f : material->indexOfRefraction;
            F32 iorRatio = n1 / n2;

            F32 cosI = -ray.direction.dot(N);
            F32 cosTheta2 = 1.0f - (iorRatio * iorRatio) * (1.0f - cosI * cosI);
            F32 Fresnel = 1.0f;

            if (cosTheta2 > 0.0f)
            {
                F32 a = n1 - n2, b = n1 + n2;
                F32 r0 = (a * a) / (b * b);
                F32 c = 1.0f - cosI;
                F32 Fresnel = r0 + (1.0f - r0) * (c * c * c * c * c);

                mustRefract = randomF32(seed) > Fresnel;
                if (mustRefract)
                    R = iorRatio * ray.direction + ((iorRatio * cosI - sqrtf(fabsf(cosTheta2))) * N);
            }

            lastSpecular = true;
            transmission *= material->baseColor * rrScale * mediumScale;
            inMedium = mustRefract ? !inMedium : inMedium;
        }
        else
        {
            F32 metallic = clamp(material->metallic, 0.0f, 1.0f);
            F32 specular = clamp(material->specular, 0.0f, 1.0f);
            F32 roughness = clamp(material->roughness, 0.0f, 1.0f);
            F32 reflectance = clamp(material->reflectance, 0.0f, 1.0f);

            Float3 V = -1.0f * ray.direction;
            R = randomOnHemisphere(seed, N);
            Float3 H = (V + R).normalize();
            F32 NV = clamp(N.dot(V), F32_EPSILON, 1.0f - F32_EPSILON);
            F32 NL = clamp(N.dot(R), F32_EPSILON, 1.0f - F32_EPSILON);
            F32 NH = clamp(N.dot(H), F32_EPSILON, 1.0f - F32_EPSILON);
            F32 VH = clamp(V.dot(H), F32_EPSILON, 1.0f - F32_EPSILON);

            Float3 F0 = Float3(0.16f * reflectance * reflectance);
            F0 = lerp(F0, material->baseColor, metallic);

            // Schlick's reflectance approximation
            F32 c = 1.0f - VH;
            RgbColor F = F0 + (1.0f - F0) * (c * c * c * c * c);

            // GGX normal distribution
            F32 a = roughness * roughness;
            F32 a2 = a * a;
            F32 NH2 = NH * NH;
            F32 b = NH2 * (a2 - 1.0f) + 1.0f;
            F32 D = a2 * F32_INV_PI / (b * b);

            // Gsmiths w/ Schlicks G1 geometry term
            F32 k = a / 2.0f;
            F32 G1L = NL / (NL * (1.0f - k) + k);
            F32 G1V = NV / (NV * (1.0f - k) + k);
            F32 G = G1L * G1V;

            RgbColor diffuseBaseColor = (1.0f - F) * (1.0f - metallic) * material->baseColor;

            RgbColor specularBrdf = (F * D * G) / (4.0f * NL * NV);
            RgbColor diffuseBrdf = diffuseBaseColor * F32_INV_PI;
            RgbColor brdf = diffuseBrdf + specularBrdf;

            F32 cosTheta = NL;
            F32 invPdf = F32_2PI;

            transmission *= cosTheta * invPdf * brdf * mediumScale * rrScale;
        }
#if 0
        if (rng < material->reflectivity)
        {
            R = reflect(ray.direction, N);
            lastSpecular = true;
            transmission *= material->albedo * rrScale * mediumScale;
        }
        else if (rng < (material->reflectivity + material->refractivity))
        {
            // Assume reflect & precalculate ray
            bool mustRefract = false;
            R = reflect(ray.direction, N);

            // Check for refraction
            F32 n1 = ray.inMedium ? material->indexOfRefraction : 1.0f;
            F32 n2 = ray.inMedium ? 1.0f : material->indexOfRefraction;
            F32 iorRatio = n1 / n2;

            F32 cosI = -ray.direction.dot(N);
            F32 cosTheta2 = 1.0f - (iorRatio * iorRatio) * (1.0f - cosI * cosI);
            F32 Fresnel = 1.0f;

            if (cosTheta2 > 0.0f)
            {
                F32 a = n1 - n2, b = n1 + n2;
                F32 r0 = (a * a) / (b * b);
                F32 c = 1.0f - cosI;
                F32 Fresnel = r0 + (1.0f - r0) * (c * c * c * c * c);

                mustRefract = randomF32(seed) > Fresnel;
                if (mustRefract)
                    R = iorRatio * ray.direction + ((iorRatio * cosI - sqrtf(fabsf(cosTheta2))) * N);
            }

            lastSpecular = true;
            transmission *= material->albedo * rrScale * mediumScale;
            inMedium = mustRefract ? !inMedium : inMedium;
        }
        else
        {
            RgbColor brdf = material->albedo * F32_INV_PI;

            if (lightCount > 0) // Can only do NEE if there are explicit lights to be sampled
            {
                const Instance& light = m_scene.sampleLights(seed);
                const SamplePoint point = light.samplePoint(seed);
                
                Float3 IL = point.position - I;
                Float3 L = IL.normalize();
                Float3 LN = point.normal;

                Float3 SO = I + F32_EPSILON * L;
                Ray sr = Ray(SO, L);
                sr.depth = IL.magnitude() - 2.0f * F32_EPSILON;

                F32 falloff = 1.0f / IL.dot(IL);
                F32 cosO = N.dot(L);
                F32 cosI = LN.dot(-1.0f * L);
                if (cosO > 0.0f && cosI > 0.0f && !m_scene.intersectAny(sr))
                {
                    F32 SA = cosI * light.area * falloff;
                    F32 lPdf = 1.0f / SA;

                    Float3 Ld = light.material->emittance() * SA * brdf * cosO * static_cast<F32>(lightCount);
                    energy += transmission * Ld;
                }
            }

            R = randomOnHemisphereCosineWeighted(seed, N);
            F32 cosTheta = N.dot(R);
            F32 invCosTheta = 1.0f / cosTheta;
            F32 invPdf = F32_PI * invCosTheta;

            lastSpecular = false;
            transmission *= cosTheta * invPdf * brdf * mediumScale * rrScale;
        }
#endif

        Float3 O = I + F32_EPSILON * R;
        ray = Ray(O, R);
        ray.inMedium = inMedium;
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
    
    VK_CHECK(vkQueueSubmit(m_context->queues.graphicsQueue.handle, 1, &oneshotSubmit, m_copyFinishedFence));
    VK_CHECK(vkWaitForFences(m_context->device, 1, &m_copyFinishedFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context->device, 1, &m_copyFinishedFence));
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

WaveFrontRenderer::WaveFrontRenderer(RenderContext* renderContext, UIManager* uiManager, RendererConfig config, FramebufferSize renderResolution, Camera& camera, GPUScene& scene)
    :
    m_context(renderContext),
    m_uiManager(uiManager),
    m_config(config),
    m_camera(camera),
    m_scene(scene),
    m_renderResolution(renderResolution)
{
    // Set up per frame structures
    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = m_context->queues.graphicsQueue.familyIndex;

        VK_CHECK(vkCreateCommandPool(m_context->device, &poolCreateInfo, nullptr, &m_frames[i].pool));

        VkCommandBufferAllocateInfo bufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferAllocateInfo.commandPool = m_frames[i].pool;
        bufferAllocateInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_context->device, &bufferAllocateInfo, &m_frames[i].presentCommandBuffer));
        VK_CHECK(vkAllocateCommandBuffers(m_context->device, &bufferAllocateInfo, &m_frames[i].uiCommandBuffer));

        VkFenceCreateInfo frameReadyCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        frameReadyCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(m_context->device, &frameReadyCreateInfo, nullptr, &m_frames[i].frameReady));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapImageAvailable));
        VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderingFinished));
        VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_frames[i].uiPassFinished));
    }

    // Set up wavefront compute structures
    VkCommandPoolCreateInfo wfPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    wfPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    wfPoolCreateInfo.queueFamilyIndex = m_context->queues.computeQueue.familyIndex;
    VK_CHECK(vkCreateCommandPool(m_context->device, &wfPoolCreateInfo, nullptr, &m_wavefrontCompute.pool));

    VkCommandBufferAllocateInfo computeBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    computeBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    computeBufferAllocateInfo.commandPool = m_wavefrontCompute.pool;
    computeBufferAllocateInfo.commandBufferCount = 3;
    VK_CHECK(vkAllocateCommandBuffers(m_context->device, &computeBufferAllocateInfo, m_wavefrontCompute.wavefrontBuffers));

    VkFenceCreateInfo computeReadyCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    computeReadyCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(m_context->device, &computeReadyCreateInfo, nullptr, &m_wavefrontCompute.computeReady));

    VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VK_CHECK(vkCreateSemaphore(m_context->device, &semaphoreCreateInfo, nullptr, &m_wavefrontCompute.computeFinished));

    // Create framebuffers for swapchain images
    m_framebuffers.reserve(m_context->swapchain.image_count);
    for (const auto& imageView : m_context->swapImageViews)
    {
        Framebuffer swapFramebuffer(m_context->device, m_presentPass, { imageView }, m_framebufferSize.width, m_framebufferSize.height);
        m_framebuffers.push_back(std::move(swapFramebuffer));
    }

    // Upload scene background data -> One off action
    m_sceneDataUBO.copyToBuffer(sizeof(SceneBackground), &m_scene.backgroundSettings());

    // Create writesets for all compute descriptors
    // Camera and frame data
    WriteDescriptorSet cameraWriteSet = {};
    cameraWriteSet.set = 0;
    cameraWriteSet.binding = 0;
    cameraWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_cameraUBO.handle(),
        0, sizeof(CameraUBO)
    };

    WriteDescriptorSet frameStateWriteSet = {};
    frameStateWriteSet.set = 0;
    frameStateWriteSet.binding = 1;
    frameStateWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameStateWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_frameStateUBO.handle(),
        0, sizeof(FrameStateUBO)
    };

    WriteDescriptorSet accumulatorWriteSet = {};
    accumulatorWriteSet.set = 0;
    accumulatorWriteSet.binding = 2;
    accumulatorWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    accumulatorWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_accumulatorSSBO.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet outputImageWriteSet = {};
    outputImageWriteSet.set = 0;
    outputImageWriteSet.binding = 3;
    outputImageWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWriteSet.imageInfo = VkDescriptorImageInfo{
        VK_NULL_HANDLE,
        m_frameImage.view(),
        VK_IMAGE_LAYOUT_GENERAL
    };

    // Acceleration structure & scene data
    WriteDescriptorSet sceneDataWriteSet = {};
    sceneDataWriteSet.set = 2;
    sceneDataWriteSet.binding = 0;
    sceneDataWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneDataWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_sceneDataUBO.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet triBufWriteset = {};
    triBufWriteset.set = 2;
    triBufWriteset.binding = 1;
    triBufWriteset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    triBufWriteset.bufferInfo = VkDescriptorBufferInfo{
        m_scene.globalTriBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet triExtBufWriteset = {};
    triExtBufWriteset.set = 2;
    triExtBufWriteset.binding = 2;
    triExtBufWriteset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    triExtBufWriteset.bufferInfo = VkDescriptorBufferInfo{
        m_scene.globalTriExtBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet blasIdxWriteSet = {};
    blasIdxWriteSet.set = 2;
    blasIdxWriteSet.binding = 3;
    blasIdxWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    blasIdxWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.BLASGlobalIndexBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet blasNodeWriteSet = {};
    blasNodeWriteSet.set = 2;
    blasNodeWriteSet.binding = 4;
    blasNodeWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    blasNodeWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.BLASGlobalNodeBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet materialWriteSet = {};
    materialWriteSet.set = 2;
    materialWriteSet.binding = 5;
    materialWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.materialBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet instanceWriteSet = {};
    instanceWriteSet.set = 2;
    instanceWriteSet.binding = 6;
    instanceWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    instanceWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.instanceBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet tlasIdxWriteSet = {};
    tlasIdxWriteSet.set = 2;
    tlasIdxWriteSet.binding = 7;
    tlasIdxWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    tlasIdxWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.TLASIndexBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet tlasNodeWriteSet = {};
    tlasNodeWriteSet.set = 2;
    tlasNodeWriteSet.binding = 8;
    tlasNodeWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    tlasNodeWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.TLASNodeBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    WriteDescriptorSet lightDataWriteSet = {};
    lightDataWriteSet.set = 2;
    lightDataWriteSet.binding = 9;
    lightDataWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightDataWriteSet.bufferInfo = VkDescriptorBufferInfo{
        m_scene.lightBuffer.handle(),
        0, VK_WHOLE_SIZE
    };

    // Wavefront data
    WriteDescriptorSet rayCounterWriteSet = {};
    rayCounterWriteSet.set = 1;
    rayCounterWriteSet.binding = 0;
    rayCounterWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rayCounterWriteSet.bufferInfo.buffer = m_rayCounters.handle();
    rayCounterWriteSet.bufferInfo.offset = 0;
    rayCounterWriteSet.bufferInfo.range = VK_WHOLE_SIZE;

    WriteDescriptorSet shadowRayCounterWriteSet = {};
    shadowRayCounterWriteSet.set = 1;
    shadowRayCounterWriteSet.binding = 3;
    shadowRayCounterWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    shadowRayCounterWriteSet.bufferInfo.buffer = m_shadowRayCounter.handle();
    shadowRayCounterWriteSet.bufferInfo.offset = 0;
    shadowRayCounterWriteSet.bufferInfo.range = VK_WHOLE_SIZE;

    WriteDescriptorSet shadowRayBufferWriteSet = {};
    shadowRayBufferWriteSet.set = 1;
    shadowRayBufferWriteSet.binding = 4;
    shadowRayBufferWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    shadowRayBufferWriteSet.bufferInfo.buffer = m_shadowRayBuffer.handle();
    shadowRayBufferWriteSet.bufferInfo.offset = 0;
    shadowRayBufferWriteSet.bufferInfo.range = VK_WHOLE_SIZE;

    // Update all compute pipeline descriptor sets
    m_rayGenPipeline.updateDescriptorSets({
        cameraWriteSet,
        frameStateWriteSet,
        rayCounterWriteSet,
    });

    m_rayExtPipeline.updateDescriptorSets({
        rayCounterWriteSet,
        triBufWriteset,
        blasIdxWriteSet, blasNodeWriteSet,
        instanceWriteSet,
        tlasIdxWriteSet, tlasNodeWriteSet,
    });

    m_rayShadePipeline.updateDescriptorSets({
        frameStateWriteSet,
        accumulatorWriteSet,
        rayCounterWriteSet,
        shadowRayCounterWriteSet,
        shadowRayBufferWriteSet,
        sceneDataWriteSet,
        triBufWriteset,
        triExtBufWriteset,
        materialWriteSet,
        instanceWriteSet,
        lightDataWriteSet,
    });

    m_rayConnectPipeline.updateDescriptorSets({
        frameStateWriteSet,
        accumulatorWriteSet,
        shadowRayCounterWriteSet,
        shadowRayBufferWriteSet,
        sceneDataWriteSet,
        triBufWriteset,
        triExtBufWriteset,
        blasIdxWriteSet, blasNodeWriteSet,
        materialWriteSet,
        instanceWriteSet,
        tlasIdxWriteSet, tlasNodeWriteSet,
        lightDataWriteSet,
    });

    m_wfFinalizePipeline.updateDescriptorSets({
        frameStateWriteSet,
        accumulatorWriteSet,
        outputImageWriteSet,
    });

    // Create write sets for graphics pipeline pass
    WriteDescriptorSet frameImageSamplerSet = {};
    frameImageSamplerSet.set = 0;
    frameImageSamplerSet.binding = 0;
    frameImageSamplerSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    frameImageSamplerSet.imageInfo = VkDescriptorImageInfo{
        m_frameImageSampler.handle(),
        m_frameImage.view(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    // Update all graphics pipeline descriptor sets
    m_presentPipeline.updateDescriptorSets({
        frameImageSamplerSet
    });

    // Finalize uses fixed descriptors -> can be prebaked
    bakeFinalizePass(m_wavefrontCompute.finalizeBuffer);
    clearAccumulator();
}

WaveFrontRenderer::~WaveFrontRenderer()
{
    vkDeviceWaitIdle(m_context->device);

    VK_CHECK(vkWaitForFences(m_context->device, 1, &m_wavefrontCompute.computeReady, VK_TRUE, UINT64_MAX));
    vkDestroyCommandPool(m_context->device, m_wavefrontCompute.pool, nullptr);
    vkDestroyFence(m_context->device, m_wavefrontCompute.computeReady, nullptr);
    vkDestroySemaphore(m_context->device, m_wavefrontCompute.computeFinished, nullptr);

    for (SizeType i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkWaitForFences(m_context->device, 1, &m_frames[i].frameReady, VK_TRUE, UINT64_MAX));
        vkDestroyCommandPool(m_context->device, m_frames[i].pool, nullptr);
        vkDestroyFence(m_context->device, m_frames[i].frameReady, nullptr);
        vkDestroySemaphore(m_context->device, m_frames[i].swapImageAvailable, nullptr);
        vkDestroySemaphore(m_context->device, m_frames[i].renderingFinished, nullptr);
        vkDestroySemaphore(m_context->device, m_frames[i].uiPassFinished, nullptr);
    }
}

void WaveFrontRenderer::clearAccumulator()
{
    vkDeviceWaitIdle(m_context->device);

    // Clear any still queued rays
    m_rayCounters.clear();

    // clear accumulator buffer
    m_frameState.totalSamples = 0;
    m_accumulatorSSBO.clear();
}

void WaveFrontRenderer::render(F32 deltaTime)
{
    const FrameData& activeFrame = m_frames[m_currentFrame];
    const WavefrontCompute& activeCompute = m_wavefrontCompute;
    
    U32 swapImageIdx = 0;
    VK_CHECK(vkAcquireNextImageKHR(m_context->device, m_context->swapchain, UINT64_MAX, activeFrame.swapImageAvailable, VK_NULL_HANDLE, &swapImageIdx));

    VkFence fences[] = { activeFrame.frameReady };
    VK_CHECK(vkWaitForFences(m_context->device, 1, fences, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context->device, 1, fences));

    // Wait on compute before updating uniforms
    VK_CHECK(vkWaitForFences(m_context->device, 1, &m_wavefrontCompute.computeReady, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_context->device, 1, &m_wavefrontCompute.computeReady));

#if WF_LUMEN_OUTPUT == 1
    // Update frame instrumentation data Lumen output
    SizeType accumulatorSize = m_renderResolution.width * m_renderResolution.height;
    F32 invSamples = 1.0f / static_cast<F32>(m_frameState.totalSamples);
    Float4* pAccumulator = nullptr;
    
    m_accumulatorSSBO.persistentMap(reinterpret_cast<void**>(&pAccumulator));
    m_frameInstrumentationData.energy = 0.0f;
    for (SizeType i = 0; i < accumulatorSize; i++)
    {
        RgbaColor color = pAccumulator[i] * invSamples;
        m_frameInstrumentationData.energy += color.r + color.g + color.b;
    }
    m_accumulatorSSBO.unmap();
#endif

    // Update cameraUBO
    CameraUBO cameraUBO = CameraUBO{
        m_camera.position,
        m_camera.up, m_camera.forward, m_camera.right(),
        m_camera.viewPlane.firstPixel, m_camera.viewPlane.uVector, m_camera.viewPlane.vVector,
        Float2(m_camera.screenWidth, m_camera.screenHeight),
        m_camera.focalLength, m_camera.defocusAngle
    };
    m_cameraUBO.copyToBuffer(sizeof(CameraUBO), &cameraUBO);

    // Update frameStateUBO
    m_frameState.samplesPerFrame = m_config.samplesPerFrame;
    m_frameState.totalSamples += m_frameState.samplesPerFrame;
    m_frameStateUBO.copyToBuffer(sizeof(FrameStateUBO), &m_frameState);

    // Record UI & present pass
    const Framebuffer& activeFramebuffer = m_framebuffers[swapImageIdx];
    VK_CHECK(vkResetCommandBuffer(activeFrame.presentCommandBuffer, /* reset flags */ 0));
    VK_CHECK(vkResetCommandBuffer(activeFrame.uiCommandBuffer, /* reset flags */ 0));
    recordPresentPass(activeFrame.presentCommandBuffer, activeFramebuffer);
    m_uiManager->recordGUIPass(activeFrame.uiCommandBuffer, swapImageIdx);

    VkPipelineStageFlags computeWaitStages[] = {
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT    // Wait until compute passes have completed
    };

    // Map ray counters during wf pass
    RayBufferCounters* pRayCounters = nullptr;
    m_rayCounters.persistentMap(reinterpret_cast<void**>(&pRayCounters));
    assert(pRayCounters != nullptr);

    // Do wf compute pass for every sample
    for (U32 sample = 0; sample < m_config.samplesPerFrame; sample++)
    {
        Buffer* rayInBuffer = &m_rayBuffer0;
        Buffer* rayOutBuffer = &m_rayBuffer1;

        WriteDescriptorSet inBufferWriteSet = {};
        inBufferWriteSet.set = 1;
        inBufferWriteSet.binding = 1;
        inBufferWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        inBufferWriteSet.bufferInfo.buffer = rayInBuffer->handle();
        inBufferWriteSet.bufferInfo.offset = 0;
        inBufferWriteSet.bufferInfo.range = VK_WHOLE_SIZE;

        WriteDescriptorSet outBufferWriteSet = {};
        outBufferWriteSet.set = 1;
        outBufferWriteSet.binding = 2;
        outBufferWriteSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        outBufferWriteSet.bufferInfo.buffer = rayOutBuffer->handle();
        outBufferWriteSet.bufferInfo.offset = 0;
        outBufferWriteSet.bufferInfo.range = VK_WHOLE_SIZE;

        m_rayGenPipeline.updateDescriptorSets({
            inBufferWriteSet,
            outBufferWriteSet,
        });

        bakeRayGenPass(activeCompute.rayGenBuffer);
        VkPipelineStageFlags rayGenWaitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
        VkSubmitInfo rayGenSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        rayGenSubmit.commandBufferCount = 1;
        rayGenSubmit.pCommandBuffers = &m_wavefrontCompute.rayGenBuffer;
        rayGenSubmit.waitSemaphoreCount = 1;
        rayGenSubmit.pWaitSemaphores = (sample > 0) ? &activeCompute.computeFinished : &activeFrame.swapImageAvailable;
        rayGenSubmit.pWaitDstStageMask = rayGenWaitStages;
        rayGenSubmit.signalSemaphoreCount = 1;
        rayGenSubmit.pSignalSemaphores = &m_wavefrontCompute.computeFinished;
        VK_CHECK(vkQueueSubmit(m_context->queues.computeQueue.handle, 1, &rayGenSubmit, activeCompute.computeReady));
        VK_CHECK(vkWaitForFences(m_context->device, 1, &activeCompute.computeReady, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(m_context->device, 1, &activeCompute.computeReady));
        
        while (pRayCounters->rayIn > 0 || pRayCounters->rayOut > 0)
        {
            U32 oldRayCount = pRayCounters->rayOut;
            swap(rayInBuffer, rayOutBuffer);
            swap(pRayCounters->rayIn, pRayCounters->rayOut);
            inBufferWriteSet.bufferInfo.buffer = rayInBuffer->handle();
            outBufferWriteSet.bufferInfo.buffer = rayOutBuffer->handle();

            m_rayExtPipeline.updateDescriptorSets({
                inBufferWriteSet,
                outBufferWriteSet,
            });

            m_rayShadePipeline.updateDescriptorSets({
                inBufferWriteSet,
                outBufferWriteSet,
            });

            bakeWavePass(activeCompute.waveBuffer, pRayCounters->rayIn);
            VkSubmitInfo waveSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            waveSubmit.commandBufferCount = 1;
            waveSubmit.pCommandBuffers = &m_wavefrontCompute.waveBuffer;
            waveSubmit.waitSemaphoreCount = 1;
            waveSubmit.pWaitSemaphores = &m_wavefrontCompute.computeFinished;
            waveSubmit.pWaitDstStageMask = computeWaitStages;
            waveSubmit.signalSemaphoreCount = 1;
            waveSubmit.pSignalSemaphores = &m_wavefrontCompute.computeFinished;
            VK_CHECK(vkQueueSubmit(m_context->queues.computeQueue.handle, 1, &waveSubmit, m_wavefrontCompute.computeReady));
            VK_CHECK(vkWaitForFences(m_context->device, 1, &m_wavefrontCompute.computeReady, VK_TRUE, UINT64_MAX));
            VK_CHECK(vkResetFences(m_context->device, 1, &m_wavefrontCompute.computeReady));

            // Check diff threshold and leave rest of rays for next frame to process
            U32 newRayCount = pRayCounters->rayOut;
            if (oldRayCount - newRayCount < WF_RAY_DIFF_THRESHOLD && newRayCount <= WF_RAY_NF_BATCH_SIZE) break;
        }
    }

    VkSubmitInfo finalizeSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    finalizeSubmit.commandBufferCount = 1;
    finalizeSubmit.pCommandBuffers = &m_wavefrontCompute.finalizeBuffer;
    finalizeSubmit.waitSemaphoreCount = 1;
    finalizeSubmit.pWaitSemaphores = &m_wavefrontCompute.computeFinished;
    finalizeSubmit.pWaitDstStageMask = computeWaitStages;
    finalizeSubmit.signalSemaphoreCount = 1;
    finalizeSubmit.pSignalSemaphores = &activeCompute.computeFinished;
    VK_CHECK(vkQueueSubmit(m_context->queues.computeQueue.handle, 1, &finalizeSubmit, m_wavefrontCompute.computeReady));

    memset(pRayCounters, 0, sizeof(RayBufferCounters));
    m_rayCounters.unmap();

    VkPipelineStageFlags gfxWaitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT   // Wait until color output has been written to signal finish
    };

    VkSubmitInfo presentSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    presentSubmit.commandBufferCount = 1;
    presentSubmit.pCommandBuffers = &activeFrame.presentCommandBuffer;
    presentSubmit.waitSemaphoreCount = 1;
    presentSubmit.pWaitSemaphores = &activeCompute.computeFinished;
    presentSubmit.pWaitDstStageMask = gfxWaitStages;
    presentSubmit.signalSemaphoreCount = 1;
    presentSubmit.pSignalSemaphores = &activeFrame.renderingFinished;

    VkSubmitInfo uiSubmit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    uiSubmit.commandBufferCount = 1;
    uiSubmit.pCommandBuffers = &activeFrame.uiCommandBuffer;
    uiSubmit.waitSemaphoreCount = 1;
    uiSubmit.pWaitSemaphores = &activeFrame.renderingFinished;
    uiSubmit.pWaitDstStageMask = gfxWaitStages;
    uiSubmit.signalSemaphoreCount = 1;
    uiSubmit.pSignalSemaphores = &activeFrame.uiPassFinished;

    VkSubmitInfo renderSubmits[] = { presentSubmit, uiSubmit };
    VK_CHECK(vkQueueSubmit(m_context->queues.graphicsQueue.handle, 2, renderSubmits, activeFrame.frameReady));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &swapImageIdx;
    presentInfo.pSwapchains = &m_context->swapchain.swapchain;
    presentInfo.pResults = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &activeFrame.uiPassFinished;

    VK_CHECK(vkQueuePresentKHR(m_context->queues.presentQueue.handle, &presentInfo));
    m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
}

void WaveFrontRenderer::bakeRayGenPass(VkCommandBuffer commandBuffer)
{
    assert(commandBuffer != VK_NULL_HANDLE);

    VkCommandBufferBeginInfo cmdBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cmdBeginInfo.flags = 0;
    cmdBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

    // Ray Gen kernel dispatch
    {
        const std::vector<VkDescriptorSet>& rayGenSets = m_rayGenPipeline.descriptorSets();
        vkCmdBindDescriptorSets(
            commandBuffer,
            m_rayGenPipeline.bindPoint(),
            m_wavefrontLayout.handle(),
            0, static_cast<U32>(rayGenSets.size()),
            rayGenSets.data(),
            0, nullptr
        );
        vkCmdBindPipeline(commandBuffer, m_rayGenPipeline.bindPoint(), m_rayGenPipeline.handle());
        vkCmdDispatch(commandBuffer, m_renderResolution.width / 32, (m_renderResolution.height / 32) + 1, 1);
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void WaveFrontRenderer::bakeWavePass(VkCommandBuffer commandBuffer, U32 rayInputSize)
{
    assert(commandBuffer != VK_NULL_HANDLE);

    VkBufferMemoryBarrier2 rayCounterBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    rayCounterBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    rayCounterBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    rayCounterBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    rayCounterBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    rayCounterBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    rayCounterBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    rayCounterBarrier.buffer = m_rayCounters.handle();
    rayCounterBarrier.offset = 0;
    rayCounterBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 buffer0Barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    buffer0Barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    buffer0Barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    buffer0Barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    buffer0Barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    buffer0Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer0Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer0Barrier.buffer = m_rayBuffer0.handle();
    buffer0Barrier.offset = 0;
    buffer0Barrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 buffer1Barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    buffer1Barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    buffer1Barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    buffer1Barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    buffer1Barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    buffer1Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer1Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer1Barrier.buffer = m_rayBuffer1.handle();
    buffer1Barrier.offset = 0;
    buffer1Barrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 accumulatorBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    accumulatorBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    accumulatorBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    accumulatorBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    accumulatorBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    accumulatorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    accumulatorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    accumulatorBarrier.buffer = m_accumulatorSSBO.handle();
    accumulatorBarrier.offset = 0;
    accumulatorBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 srCounterBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    srCounterBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    srCounterBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    srCounterBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    srCounterBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    srCounterBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srCounterBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srCounterBarrier.buffer = m_shadowRayCounter.handle();
    srCounterBarrier.offset = 0;
    srCounterBarrier.size = VK_WHOLE_SIZE;

    VkBufferMemoryBarrier2 srBufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
    srBufferBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    srBufferBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    srBufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    srBufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    srBufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srBufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srBufferBarrier.buffer = m_shadowRayBuffer.handle();
    srBufferBarrier.offset = 0;
    srBufferBarrier.size = VK_WHOLE_SIZE;

    VkCommandBufferBeginInfo cmdBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cmdBeginInfo.flags = 0;
    cmdBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

    // Extend kernel dispatch
    {
        const std::vector<VkDescriptorSet>& sets = m_rayExtPipeline.descriptorSets();
        vkCmdBindDescriptorSets(
            commandBuffer,
            m_rayExtPipeline.bindPoint(),
            m_wavefrontLayout.handle(),
            0, static_cast<U32>(sets.size()),
            sets.data(),
            0, nullptr
        );
        vkCmdBindPipeline(commandBuffer, m_rayExtPipeline.bindPoint(), m_rayExtPipeline.handle());
        vkCmdDispatch(commandBuffer, rayInputSize / 32 + 1, 1, 1);
    }

    VkBufferMemoryBarrier2 extendShadeBufferBarriers[] = {
        rayCounterBarrier,
        buffer0Barrier,
        buffer1Barrier,
    };

    VkDependencyInfo extendShadeDependency = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    extendShadeDependency.bufferMemoryBarrierCount = 3;
    extendShadeDependency.pBufferMemoryBarriers = extendShadeBufferBarriers;
    vkCmdPipelineBarrier2(commandBuffer, &extendShadeDependency);

    // shade kernel dispatch
    {
        const std::vector<VkDescriptorSet>& sets = m_rayShadePipeline.descriptorSets();
        vkCmdBindDescriptorSets(
            commandBuffer,
            m_rayShadePipeline.bindPoint(),
            m_wavefrontLayout.handle(),
            0, static_cast<U32>(sets.size()),
            sets.data(),
            0, nullptr
        );
        vkCmdBindPipeline(commandBuffer, m_rayShadePipeline.bindPoint(), m_rayShadePipeline.handle());
        vkCmdDispatch(commandBuffer, m_renderResolution.width / 32 + 1, m_renderResolution.height / 32 + 1, 1);
    }

    VkBufferMemoryBarrier2 shadeConnectBufferBarriers[] = {
        accumulatorBarrier,
        srCounterBarrier,
        srBufferBarrier,
    };

    VkDependencyInfo shadeConnectDependency = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    extendShadeDependency.bufferMemoryBarrierCount = 3;
    extendShadeDependency.pBufferMemoryBarriers = shadeConnectBufferBarriers;
    vkCmdPipelineBarrier2(commandBuffer, &shadeConnectDependency);

    // connect kernel dispatch
    {
        const std::vector<VkDescriptorSet>& sets = m_rayConnectPipeline.descriptorSets();
        vkCmdBindDescriptorSets(
            commandBuffer,
            m_rayConnectPipeline.bindPoint(),
            m_wavefrontLayout.handle(),
            0, static_cast<U32>(sets.size()),
            sets.data(),
            0, nullptr
        );
        vkCmdBindPipeline(commandBuffer, m_rayConnectPipeline.bindPoint(), m_rayConnectPipeline.handle());
        vkCmdDispatch(commandBuffer, m_renderResolution.width / 32 + 1, m_renderResolution.height / 32 + 1, 1);
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void WaveFrontRenderer::bakeFinalizePass(VkCommandBuffer commandBuffer)
{
    assert(commandBuffer != VK_NULL_HANDLE);

    VkCommandBufferBeginInfo cmdBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cmdBeginInfo.flags = 0;
    cmdBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

    VkImageMemoryBarrier2 storageImageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    storageImageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    storageImageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    storageImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    storageImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    storageImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    storageImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageImageBarrier.image = m_frameImage.handle();
    storageImageBarrier.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,
        0, 1
    };

    VkDependencyInfo finalizeDependencies = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    finalizeDependencies.imageMemoryBarrierCount = 1;
    finalizeDependencies.pImageMemoryBarriers = &storageImageBarrier;
    vkCmdPipelineBarrier2(commandBuffer, &finalizeDependencies);

    // finalize kernel dispatch
    {
        const std::vector<VkDescriptorSet>& sets = m_wfFinalizePipeline.descriptorSets();
        vkCmdBindDescriptorSets(
            commandBuffer,
            m_wfFinalizePipeline.bindPoint(),
            m_wavefrontLayout.handle(),
            0, static_cast<U32>(sets.size()),
            sets.data(),
            0, nullptr
        );
        vkCmdBindPipeline(commandBuffer, m_wfFinalizePipeline.bindPoint(), m_wfFinalizePipeline.handle());
        vkCmdDispatch(commandBuffer, m_renderResolution.width / 32, (m_renderResolution.height / 32) + 1, 1);
    }

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void WaveFrontRenderer::recordPresentPass(VkCommandBuffer commandBuffer, const Framebuffer& framebuffer)
{
    assert(commandBuffer != VK_NULL_HANDLE);
    
    VkCommandBufferBeginInfo cmdBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cmdBeginInfo.flags = 0;
    cmdBeginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

    VkImageMemoryBarrier2 shaderReadBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    shaderReadBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    shaderReadBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    shaderReadBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    shaderReadBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    shaderReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shaderReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shaderReadBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    shaderReadBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shaderReadBarrier.image = m_frameImage.handle();
    shaderReadBarrier.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,
        0, 1
    };

    VkDependencyInfo shaderDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    shaderDepInfo.imageMemoryBarrierCount = 1;
    shaderDepInfo.pImageMemoryBarriers = &shaderReadBarrier;
    vkCmdPipelineBarrier2(commandBuffer, &shaderDepInfo);

    VkClearValue clearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassBeginInfo.renderPass = m_presentPass.handle();
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
        m_presentLayout.handle(),
        0, static_cast<U32>(descriptorSets.size()),
        descriptorSets.data(),
        0, nullptr
    );

    vkCmdBindPipeline(commandBuffer, m_presentPipeline.bindPoint(), m_presentPipeline.handle());
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);   // Single instance -> shader takes vertex index and transforms it to screen coords

    vkCmdEndRenderPass(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}
