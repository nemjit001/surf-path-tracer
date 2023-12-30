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
    VkDescriptorImageInfo imageInfo;
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

class GraphicsPipeline
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

    ~GraphicsPipeline();

    GraphicsPipeline(GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(GraphicsPipeline&) = delete;

    GraphicsPipeline(GraphicsPipeline&& other) noexcept;
    GraphicsPipeline& operator=(GraphicsPipeline&& other) noexcept;

    void updateDescriptorSets(const std::vector<WriteDescriptorSet>& sets);

    VkPipelineBindPoint bindPoint() const;

    VkPipeline handle() const;

    const std::vector<VkDescriptorSet>& descriptorSets();

private:
    void release();

private:
    VkDevice m_device;
    VkPipeline m_pipeline;
    std::vector<VkDescriptorSet> m_descriptorSets;
};
