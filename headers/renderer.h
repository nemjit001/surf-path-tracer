#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_context.h"
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

struct RenderResulution
{
    U32 width;
    U32 height;
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
    Renderer(RenderContext renderContext, RenderResulution resolution, PixelBuffer resultBuffer);

    ~Renderer();

    void render(F32 deltaTime);

private:
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
    PixelBuffer m_resultBuffer;

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
