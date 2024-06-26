#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "camera.h"
#include "pixel_buffer.h"
#include "ray.h"
#include "render_context.h"
#include "scene.h"
#include "surf_math.h"
#include "types.h"
#include "ui_manager.h"
#include "vk_layer/buffer.h"
#include "vk_layer/descriptor_pool.h"
#include "vk_layer/framebuffer.h"
#include "vk_layer/image.h"
#include "vk_layer/pipeline.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/sampler.h"

#define FRAMES_IN_FLIGHT    3

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

struct FrameStateUBO
{
    ALIGN(4) U32 samplesPerFrame     = 0;
    ALIGN(4) U32 totalSamples        = 0;
};

struct RayBufferCounters
{
    ALIGN(4) I32 rayIn;
    ALIGN(4) I32 rayOut;
};

struct ShadowRayCounter
{
    ALIGN(4) I32 rayCount;
    ALIGN(4) bool extendBuffer;
};

struct FrameData
{
    VkCommandPool pool;
    VkCommandBuffer presentCommandBuffer;
    VkCommandBuffer uiCommandBuffer;
    VkFence frameReady;
    VkSemaphore swapImageAvailable;
    VkSemaphore uiPassFinished;
    VkSemaphore renderingFinished;
};

struct WavefrontCompute
{
    VkCommandPool pool;
    union {
        struct { VkCommandBuffer rayGenBuffer, waveBuffer, finalizeBuffer; };
        VkCommandBuffer wavefrontBuffers[3];
    };
    VkFence computeReady;
    VkSemaphore computeFinished;
};

class IRenderer
{
public:
    virtual void clearAccumulator() = 0;

    virtual void render(F32 deltaTime) = 0;

    virtual RendererConfig& config() = 0;

    virtual const FrameInstrumentationData& frameInfo() = 0;
};

class Renderer
    : public IRenderer
{
public:
    Renderer(RenderContext* renderContext, UIManager* uiManager, RendererConfig config, PixelBuffer resultBuffer, Camera& camera, Scene& scene);

    ~Renderer();

    virtual void clearAccumulator() override;

    virtual void render(F32 deltaTime) override;

    virtual inline RendererConfig& config() override { return m_config; }

    virtual inline const FrameInstrumentationData& frameInfo() override { return m_frameInstrumentationData; }

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
    RenderContext* m_context;
    UIManager* m_uiManager;
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
    RenderPass m_presentPass = RenderPass(
        m_context->device,
        std::vector{
            ImageAttachment{
                m_context->swapchain.image_format, VK_SAMPLE_COUNT_1_BIT,
                ImageOps{ VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE },
                ImageOps{ VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE },
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
        },
        std::vector{
            AttachmentReference{ AttachmentType::Color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
        }
    );
    std::vector<Framebuffer> m_framebuffers = std::vector<Framebuffer>();

    // Rendered frame staging buffer & target image
    Buffer m_frameStagingBuffer = Buffer(
        m_context->allocator,
        m_resultBuffer.width * m_resultBuffer.height * sizeof(U32),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,            // Used as staging bufer for GPU uploads
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT       // Needs to be visible from the CPU
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,   // And always coherent with CPU memory during uploads
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT // Should allow memcpy writes & mapping
    );

    Image m_frameImage = Image(
        m_context->device,
        m_context->allocator,
        VkFormat::VK_FORMAT_R8G8B8A8_UNORM,                     // Standard RGBA format
        m_resultBuffer.width, m_resultBuffer.height,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT   // Used as transfer destination for CPU staging buffer
        | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT      // Used in present shader as sampled screen texture
    );

    // Custom sampler, shader pipeline & layout for use during render pass
    Sampler m_frameImageSampler = Sampler(m_context->device);

    PipelineLayout m_presentPipelineLayout = PipelineLayout(m_context->device, std::vector{
        DescriptorSetLayout{
            std::vector{ DescriptorSetBinding{ 0, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER } }
        }
    });

    Shader m_presentVertShader = Shader(m_context->device, ShaderType::Vertex, "shaders/fs_quad.vert.spv");
    Shader m_presentFragShader = Shader(m_context->device, ShaderType::Fragment, "shaders/fs_quad.frag.spv");

    GraphicsPipeline m_presentPipeline = GraphicsPipeline(
        m_context->device, Viewport{ 0, 0, m_framebufferSize.width, m_framebufferSize.height, 0.0f, 1.0f },
        m_descriptorPool, m_presentPass, m_presentPipelineLayout,
        std::vector{ &m_presentVertShader, &m_presentFragShader }
    );
};

class WaveFrontRenderer
    : public IRenderer
{
public:
    WaveFrontRenderer(RenderContext* renderContext, UIManager* uiManager, RendererConfig config, FramebufferSize renderResolution, Camera& camera, GPUScene& scene);

    ~WaveFrontRenderer();

    virtual void clearAccumulator() override;

    virtual void render(F32 deltaTime) override;

    virtual inline RendererConfig& config() override { return m_config; }

    virtual const FrameInstrumentationData& frameInfo() override
    {
        // update total samples before returning
        m_frameInstrumentationData.totalSamples = m_frameState.totalSamples;
        return m_frameInstrumentationData;
    }

private:
    void bakeRayGenPass(VkCommandBuffer commandBuffer);

    void bakeWavePass(VkCommandBuffer commandBuffer);

    void bakeFinalizePass(VkCommandBuffer commandBuffer);

    void recordPresentPass(
        VkCommandBuffer commandBuffer,
        const Framebuffer& framebuffer
    );

private:
    RenderContext* m_context;
    UIManager* m_uiManager;
    RendererConfig m_config;
    Camera& m_camera;
    GPUScene& m_scene;

    // Frame data
    FramebufferSize m_renderResolution;
    FrameStateUBO m_frameState = FrameStateUBO{};
    FrameInstrumentationData m_frameInstrumentationData = FrameInstrumentationData{};

    // Frame management
    SizeType m_currentFrame = 0;
    FramebufferSize m_framebufferSize = m_context->getFramebufferSize();
    FrameData m_frames[FRAMES_IN_FLIGHT] = {};
    WavefrontCompute m_wavefrontCompute = {};

    // Default render pass w/ framebuffers
    RenderPass m_presentPass = RenderPass(
        m_context->device,
        std::vector{
            ImageAttachment{
                m_context->swapchain.image_format, VK_SAMPLE_COUNT_1_BIT,
                ImageOps{ VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE },
                ImageOps{ VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE },
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
        },
        std::vector{
            AttachmentReference{ AttachmentType::Color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
        }
    );
    std::vector<Framebuffer> m_framebuffers = std::vector<Framebuffer>();

    // Default descriptor pool
    DescriptorPool m_descriptorPool = DescriptorPool(m_context->device);

    // Wavefront kernels
    Shader m_rayGeneration  = Shader(m_context->device, ShaderType::Compute, "shaders/ray_generation.comp.spv");
    Shader m_rayExtend      = Shader(m_context->device, ShaderType::Compute, "shaders/ray_extend.comp.spv");
    Shader m_rayShade       = Shader(m_context->device, ShaderType::Compute, "shaders/ray_shade.comp.spv");
    Shader m_rayConnect     = Shader(m_context->device, ShaderType::Compute, "shaders/ray_connect.comp.spv");
    Shader m_wfFinalize     = Shader(m_context->device, ShaderType::Compute, "shaders/wavefront_finalize.comp.spv");

    // Wavefront layout & pipelines
    PipelineLayout m_wavefrontLayout = PipelineLayout(m_context->device, std::vector{
        DescriptorSetLayout{    // Per frame data uniforms (camera, frame state, accumulator, output image)
            std::vector{
                DescriptorSetBinding{ 0, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
                DescriptorSetBinding{ 1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
                DescriptorSetBinding{ 2, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 3, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
            }
        },
        DescriptorSetLayout{    // Wavefront compute SSBOs (GPU counters, rayBuffers 0 & 1, shadow ray counter, shadow ray buffer, material buffer)
            std::vector{
                DescriptorSetBinding{ 0, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 2, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 3, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 4, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 5, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
            }
        },
        DescriptorSetLayout{    // Scene UBOs & SSBOs
            std::vector{
                DescriptorSetBinding{ 0, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
                DescriptorSetBinding{ 1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 2, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 3, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 4, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 5, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 6, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 7, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 8, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
                DescriptorSetBinding{ 9, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
            }
        },
    });

    ComputePipeline m_rayGenPipeline        = ComputePipeline(m_context->device, m_descriptorPool, m_wavefrontLayout, &m_rayGeneration);
    ComputePipeline m_rayExtPipeline        = ComputePipeline(m_context->device, m_descriptorPool, m_wavefrontLayout, &m_rayExtend);
    ComputePipeline m_rayShadePipeline      = ComputePipeline(m_context->device, m_descriptorPool, m_wavefrontLayout, &m_rayShade);
    ComputePipeline m_rayConnectPipeline    = ComputePipeline(m_context->device, m_descriptorPool, m_wavefrontLayout, &m_rayConnect);
    ComputePipeline m_wfFinalizePipeline    = ComputePipeline(m_context->device, m_descriptorPool, m_wavefrontLayout, &m_wfFinalize);

    // Compute descriptors
    Buffer m_cameraUBO = Buffer(
        m_context->allocator, sizeof(CameraUBO),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    Buffer m_frameStateUBO = Buffer(
        m_context->allocator, sizeof(FrameStateUBO),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    Buffer m_accumulatorSSBO = Buffer(
        m_context->allocator, m_renderResolution.width * m_renderResolution.height * sizeof(Float4),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    Buffer m_sceneDataUBO = Buffer(
        m_context->allocator, sizeof(SceneBackground),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    // Size is ceil(1.5 * render resolution) to ensure large enough buffers for generated rays in flight
    const SizeType c_rayBufferSize = static_cast<SizeType>(1.5f * m_renderResolution.width * m_renderResolution.height * sizeof(GPURay)) + 1;
    SizeType m_shadowRayBufferSize = static_cast<SizeType>(1.5f * m_renderResolution.width * m_renderResolution.height * sizeof(GPUShadowRayMetadata)) + 1;
    Buffer m_rayCounters = Buffer(
        m_context->allocator, sizeof(RayBufferCounters),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    Buffer m_rayBuffer0 = Buffer(
        m_context->allocator, c_rayBufferSize,
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0
    );

    Buffer m_rayBuffer1 = Buffer(
        m_context->allocator, c_rayBufferSize,
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0
    );

    Buffer m_shadowRayCounter = Buffer(
        m_context->allocator, sizeof(ShadowRayCounter),
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    Buffer m_shadowRayBuffer = Buffer(
        m_context->allocator, m_shadowRayBufferSize,
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0
    );

    Buffer m_materialEvalRayBuffer = Buffer(
        m_context->allocator, (4 * sizeof(U32)) + c_rayBufferSize,
        VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0
    );

    // Present shaders
    Shader m_presentVert    = Shader(m_context->device, ShaderType::Vertex, "shaders/fs_quad.vert.spv");
    Shader m_presentFrag    = Shader(m_context->device, ShaderType::Fragment, "shaders/fs_quad.frag.spv");

    // Present layout & pipeline
    PipelineLayout m_presentLayout = PipelineLayout(m_context->device, std::vector{
        DescriptorSetLayout{
            std::vector{
                DescriptorSetBinding{ 0, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }
            }
        }
    });

    GraphicsPipeline m_presentPipeline  = GraphicsPipeline(
        m_context->device, Viewport{ 0, 0, m_framebufferSize.width, m_framebufferSize.height, 0.0f, 1.0f },
        m_descriptorPool, m_presentPass, m_presentLayout,
        { &m_presentVert, &m_presentFrag }
    );

    // Present descriptors
    Sampler m_frameImageSampler = Sampler(m_context->device);
    Image m_frameImage = Image(
        m_context->device,
        m_context->allocator,
        VkFormat::VK_FORMAT_R8G8B8A8_UNORM,                     // Standard RGBA format (Scaled normalized)
        m_renderResolution.width, m_renderResolution.height,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT        // Used as storage for wavefront compute shaders
        | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT      // Used in present shader as sampled screen texture
    );
};
