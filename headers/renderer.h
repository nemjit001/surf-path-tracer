#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_context.h"
#include "types.h"
#include "surface.h"
#include "vk_layer/framebuffer.h"
#include "vk_layer/pipeline.h"
#include "vk_layer/render_pass.h"

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
    Renderer(RenderContext renderContext, RenderResulution resolution, Surface surface);

    ~Renderer();

    void recordFrame(
        VkCommandBuffer commandBuffer,
        const Framebuffer& framebuffer,
        const RenderPass& renderPass
    );

    void render(F32 deltaTime);

private:
    RenderContext m_context;
    Surface m_renderResult;

    // Renderer Frame management
    SizeType m_currentFrame;
    FrameData m_frames[FRAMES_IN_FLIGHT];

    // Default render pass w/ framebuffers
    RenderPass m_presentPass;
    std::vector<Framebuffer> m_framebuffers;

    // Custom shader pipeline & layout for use during render pass
    PipelineLayout m_presentPipelineLayout;
    GraphicsPipeline m_presentPipeline;
};
