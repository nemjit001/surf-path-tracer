#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/descriptor_pool.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/shader.h"

struct Viewport
{
    I32 x;
    I32 y;
    U32 width;
    U32 height;
    F32 minDepth;
    F32 maxDepth;
};

struct DescriptorSetBinding
{
    U32 binding;
    VkShaderStageFlags shaderStage;
    VkDescriptorType descriptorType;
};

struct DescriptorSetLayout
{
    std::vector<DescriptorSetBinding> bindings;
};

struct WriteDescriptorSet
{
    U32 set;
    U32 binding;
    VkDescriptorType descriptorType;
    union {
        VkDescriptorImageInfo imageInfo;
        VkDescriptorBufferInfo bufferInfo;
    };
};

class PipelineLayout
{
public:
    PipelineLayout(VkDevice device, std::vector<DescriptorSetLayout> setLayouts);

    ~PipelineLayout();

    PipelineLayout(PipelineLayout&) = delete;
    PipelineLayout& operator=(PipelineLayout&) = delete;

    PipelineLayout(PipelineLayout&& other) noexcept;
    PipelineLayout& operator=(PipelineLayout&& other) noexcept;

    VkPipelineLayout handle() const;

    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts() const;

private:
    void release();

private:
    VkDevice m_device;
    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    VkPipelineLayout m_layout;
};

class IPipeline
{
public:
    IPipeline(VkDevice device);

    virtual ~IPipeline() { release(); }

    IPipeline(IPipeline&) = delete;
    IPipeline& operator=(IPipeline&) = delete;

    IPipeline(IPipeline&& other) noexcept;
    IPipeline& operator=(IPipeline&& other) noexcept;

    virtual VkPipelineBindPoint bindPoint() const = 0;

    virtual inline VkPipeline handle() const { return m_pipeline; };

    virtual inline const std::vector<VkDescriptorSet>& descriptorSets() { return m_descriptorSets; };

    void updateDescriptorSets(const std::vector<WriteDescriptorSet>& sets);

protected:
    void release();

protected:
    VkDevice m_device;
    VkPipeline m_pipeline;
    std::vector<VkDescriptorSet> m_descriptorSets;
};

class GraphicsPipeline
    : public IPipeline
{
public:
    GraphicsPipeline(
        VkDevice device,
        Viewport viewport,
        const DescriptorPool& descriptorPool,
        const RenderPass& renderPass,
        const PipelineLayout& layout,
        const std::vector<Shader*>& shaders
    );

    virtual ~GraphicsPipeline() = default;

    virtual inline VkPipelineBindPoint bindPoint() const override { return VK_PIPELINE_BIND_POINT_GRAPHICS; };
};

class ComputePipeline
    : public IPipeline
{
public:
    ComputePipeline(
        VkDevice device,
        const DescriptorPool& descriptorPool,
        const PipelineLayout& layout,
        const Shader* shader
    );

    virtual ~ComputePipeline() = default;

    virtual inline VkPipelineBindPoint bindPoint() const override { return VK_PIPELINE_BIND_POINT_COMPUTE; };
};
