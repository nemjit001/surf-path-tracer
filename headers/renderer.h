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
    SizeType totalSamples   = 0;
    SizeType bufferSize     = 0;
    RgbaColor* buffer       = nullptr;

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
    Camera& m_camera;
    Scene& m_scene;
    PixelBuffer m_resultBuffer;

    // Frame accumulator & instrumentation data
    AccumulatorState m_accumulator = AccumulatorState(m_resultBuffer.width, m_resultBuffer.height);
    FrameInstrumentationData m_frameInstrumentationData = FrameInstrumentationData{};

    // Setup for copy operations
    VkFence m_copyFinishedFence = VK_NULL_HANDLE;
    VkCommandPool m_copyPool = VK_NULL_HANDLE;
    VkCommandBuffer m_oneshotCopyBuffer = VK_NULL_HANDLE;

    // Renderer Frame management
    SizeType m_currentFrame = 0;
    FrameData m_frames[FRAMES_IN_FLIGHT] = {};

    // Default render pass w/ framebuffers
    RenderPass m_presentPass = RenderPass(m_context.device, m_context.swapchain.image_format);
    std::vector<Framebuffer> m_framebuffers = std::vector<Framebuffer>();

    // Rendered frame staging buffer & target image
    Buffer m_frameStagingBuffer = Buffer(
        m_context.allocator,
        m_resultBuffer.width * m_resultBuffer.height * sizeof(U32),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,            // Used as staging bufer for GPU uploads
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT       // Needs to be visible from the CPU
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,   // And always coherent with CPU memory during uploads
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT // Should allow memcpy writes & mapping
    );

    Image m_frameImage = Image(
        m_context.device,
        m_context.allocator,
        VkFormat::VK_FORMAT_R8G8B8A8_SRGB,                      // Standard RGBA format
        m_resultBuffer.width, m_resultBuffer.height,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT   // Used as transfer destination for CPU staging buffer
        | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT      // Used in present shader as sampled screen texture
    );

    // Custom sampler, shader pipeline & layout for use during render pass
    Sampler m_frameImageSampler = Sampler(m_context.device);

    PipelineLayout m_presentPipelineLayout = PipelineLayout(m_context.device, std::vector{
        DescriptorSetLayout{
            std::vector{ DescriptorSetBinding{ 0, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER } }
        }
    });

    Shader m_presentVertShader = Shader(m_context.device, ShaderType::Vertex, "shaders/fs_quad.vert.spv");
    Shader m_presentFragShader = Shader(m_context.device, ShaderType::Fragment, "shaders/fs_quad.frag.spv");

    GraphicsPipeline m_presentPipeline = GraphicsPipeline(
        m_context.device, Viewport{ 0, 0, m_framebufferSize.width, m_framebufferSize.height, 0.0f, 1.0f },
        m_descriptorPool, m_presentPass, m_presentPipelineLayout,
        std::vector{ &m_presentVertShader, &m_presentFragShader }
    );
};

class WaveFrontRenderer
{
public:
    WaveFrontRenderer(RenderContext renderContext, Camera& camera, Scene& scene);

    ~WaveFrontRenderer();

private:
    RenderContext m_context;
    Camera& m_camera;
    Scene& m_scene;

    // Frame management
    SizeType m_currentFrame = 0;
    FramebufferSize m_framebufferSize = m_context.getFramebufferSize();
    FrameData m_frames[FRAMES_IN_FLIGHT] = {};
    std::vector<Framebuffer> m_framebuffers = std::vector<Framebuffer>();
};

RendererConfig& Renderer::config()
{
    return m_config;
}

const FrameInstrumentationData& Renderer::frameInfo()
{
    return m_frameInstrumentationData;
}
