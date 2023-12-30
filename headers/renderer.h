#pragma once

#include <vector>
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

#define FRAMES_IN_FLIGHT 2

struct RendererConfig
{
    U32 maxBounces          = 5;
    U32 samplesPerFrame     = 1;
};

struct FrameInstrumentationData
{
    F32 energy              = 0.0f;
    U32 totalSamples        = 0;
};

struct AccumulatorState
{
    SizeType totalSamples;
    SizeType bufferSize;
    RgbaColor* buffer;

    AccumulatorState(U32 width, U32 height);

    ~AccumulatorState();
};

struct FrameData
{
    VkCommandPool pool;
    VkCommandBuffer commandBuffer;
    VkFence frameReady;
    VkSemaphore swapImageAvailable;
    VkSemaphore renderingFinished;
};

class Renderer
{
public:
    Renderer(RenderContext renderContext, RendererConfig config, PixelBuffer resultBuffer, Camera& camera, Scene& scene);

    ~Renderer();

    void clearAccumulator();

    void render(F32 deltaTime);

    inline RendererConfig& config();

    inline const FrameInstrumentationData& frameInfo();

private:
    RgbColor trace(U32& seed, Ray& ray, U32 depth = 0);

    void copyBufferToImage(
        const Buffer& staging,
        const Image& target
    );

    void recordFrame(
        VkCommandBuffer commandBuffer,
        const Framebuffer& framebuffer,
        const RenderPass& renderPass
    );

private:
    RenderContext m_context;
    DescriptorPool m_descriptorPool;
    FramebufferSize m_framebufferSize;
    RendererConfig m_config;
    PixelBuffer m_resultBuffer;
    AccumulatorState m_accumulator;
    Camera& m_camera;
    Scene& m_scene;

    // Frame instrumentation data
    FrameInstrumentationData m_frameInstrumentationData;

    // Setup for copy operations
    VkFence m_copyFinishedFence;
    VkCommandPool m_copyPool;
    VkCommandBuffer m_oneshotCopyBuffer;

    // Renderer Frame management
    SizeType m_currentFrame;
    FrameData m_frames[FRAMES_IN_FLIGHT];

    // Default render pass w/ framebuffers
    RenderPass m_presentPass;
    std::vector<Framebuffer> m_framebuffers;

    // Rendered frame staging buffer & target image
    Buffer m_frameStagingBuffer;
    Image m_frameImage;

    // Custom sampler, shader pipeline & layout for use during render pass
    Sampler m_frameImageSampler;
    PipelineLayout m_presentPipelineLayout;
    GraphicsPipeline m_presentPipeline;
};

class WaveFrontRenderer
{
public:
    WaveFrontRenderer(RenderContext renderContext, Camera& camera, Scene& scene);

    ~WaveFrontRenderer();

private:
    RenderContext m_context;
    FramebufferSize m_framebufferSize;
    Camera& m_camera;
    Scene& m_scene;

    // Frame management
    SizeType m_currentFrame;
    FrameData m_frames[FRAMES_IN_FLIGHT];
    std::vector<Framebuffer> m_framebuffers;
};

RendererConfig& Renderer::config()
{
    return m_config;
}

const FrameInstrumentationData& Renderer::frameInfo()
{
    return m_frameInstrumentationData;
}
