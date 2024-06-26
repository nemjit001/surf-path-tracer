#include "vk_layer/pipeline.h"

#include <vector>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/descriptor_pool.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/shader.h"
#include "vk_layer/vk_check.h"

PipelineLayout::PipelineLayout(VkDevice device, std::vector<DescriptorSetLayout> setLayouts)
    :
    m_device(device),
    m_layout(VK_NULL_HANDLE)
{
    SizeType descriptorSetCount = setLayouts.size();
    m_descriptorSetLayouts.resize(descriptorSetCount, VK_NULL_HANDLE);
    for (SizeType i = 0; i < descriptorSetCount; i++)
    {
        DescriptorSetLayout& layoutInfo = setLayouts[i];

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(layoutInfo.bindings.size());
        for (auto const& bindingInfo : layoutInfo.bindings)
        {
            VkDescriptorSetLayoutBinding layoutBinding = {};
            layoutBinding.binding = bindingInfo.binding;
            layoutBinding.stageFlags = bindingInfo.shaderStage;
            layoutBinding.descriptorCount = 1;
            layoutBinding.descriptorType = bindingInfo.descriptorType;
            layoutBinding.pImmutableSamplers = nullptr;

            bindings.push_back(layoutBinding);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descriptorSetLayoutCreateInfo.flags = 0;
        descriptorSetLayoutCreateInfo.bindingCount = static_cast<U32>(bindings.size());
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayouts[i]));
    }

    VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges = nullptr;
    createInfo.setLayoutCount = static_cast<U32>(m_descriptorSetLayouts.size());
    createInfo.pSetLayouts = m_descriptorSetLayouts.data();

    VK_CHECK(vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_layout));
}

PipelineLayout::~PipelineLayout()
{
    release();
}

PipelineLayout::PipelineLayout(PipelineLayout&& other) noexcept
    :
    m_device(other.m_device),
    m_layout(other.m_layout)
{
    other.m_layout = VK_NULL_HANDLE;
}

PipelineLayout& PipelineLayout::operator=(PipelineLayout&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_device = other.m_device;
    this->m_layout = other.m_layout;

    return *this;
}

VkPipelineLayout PipelineLayout::handle() const
{
    return m_layout;
}

const std::vector<VkDescriptorSetLayout>& PipelineLayout::descriptorSetLayouts() const
{
    return m_descriptorSetLayouts;
}

void PipelineLayout::release()
{
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);

    for (auto const& setLayout : m_descriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(m_device, setLayout, nullptr);
    }
}

IPipeline::IPipeline(VkDevice device)
    :
    m_device(device),
    m_pipeline(VK_NULL_HANDLE),
    m_descriptorSets()
{
    assert(m_device != VK_NULL_HANDLE);
}

IPipeline::IPipeline(IPipeline && other) noexcept
    :
    m_device(other.m_device),
    m_pipeline(other.m_pipeline),
    m_descriptorSets(other.m_descriptorSets)
{
    other.m_pipeline = VK_NULL_HANDLE;
    other.m_descriptorSets.clear();
}

IPipeline& IPipeline::operator=(IPipeline&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_device = other.m_device;
    this->m_pipeline = other.m_pipeline;
    this->m_descriptorSets = other.m_descriptorSets;

    other.m_pipeline = VK_NULL_HANDLE;
    other.m_descriptorSets.clear();

    return *this;
}

void IPipeline::updateDescriptorSets(const std::vector<WriteDescriptorSet>& sets)
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    writeDescriptorSets.reserve(sets.size());

    for (auto const& set : sets)
    {
        assert(set.set < m_descriptorSets.size());

        VkWriteDescriptorSet writeSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writeSet.dstSet = m_descriptorSets[set.set];
        writeSet.dstBinding = set.binding;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = set.descriptorType;

        switch (set.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            writeSet.pImageInfo = &set.imageInfo;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            // TODO: handle pTexelInfo
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            writeSet.pBufferInfo = &set.bufferInfo;
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        default:
            fprintf(stderr, "Cannot handle input attachment descriptor set\n");
            abort();
            break;
        }

        writeDescriptorSets.push_back(writeSet);
    }

    vkUpdateDescriptorSets(
        m_device,
        static_cast<U32>(writeDescriptorSets.size()),
        writeDescriptorSets.data(),
        0,
        nullptr
    );
}

void IPipeline::release()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
}

GraphicsPipeline::GraphicsPipeline(
    VkDevice device,
    Viewport viewport,
    const DescriptorPool& descriptorPool,
    const RenderPass& renderPass,
    const PipelineLayout& layout,
    const std::vector<Shader*>& shaders
)
    :
    IPipeline(device)
{
    std::vector<VkPipelineShaderStageCreateInfo> stages = {};
    for (auto const& shader : shaders)
    {
        VkPipelineShaderStageCreateInfo shaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        shaderStage.flags = 0;
        shaderStage.stage = shader->stage();
        shaderStage.module = shader->handle();
        shaderStage.pName = "main";
        shaderStage.pSpecializationInfo = nullptr;

        stages.push_back(shaderStage);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputState.flags = 0;
    vertexInputState.vertexAttributeDescriptionCount = 0;
    vertexInputState.pVertexAttributeDescriptions = nullptr;
    vertexInputState.vertexBindingDescriptionCount = 0;
    vertexInputState.pVertexBindingDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssemblyState.flags = 0;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    VkRect2D scissors[] = {
        VkRect2D{
            { viewport.x, viewport.y },
            { viewport.width, viewport.height }
        },
    };

    VkViewport viewports[] = {
        VkViewport {
            static_cast<F32>(viewport.x),
            static_cast<F32>(viewport.y),
            static_cast<F32>(viewport.width),
            static_cast<F32>(viewport.height),
            viewport.minDepth,
            viewport.maxDepth
        }
    };

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.flags = 0;
    viewportState.scissorCount = 1;
    viewportState.pScissors = scissors;
    viewportState.viewportCount = 1;
    viewportState.pViewports = viewports;

    VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizationState.flags = 0;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.depthBiasClamp = 0.0f;
    rasterizationState.depthBiasSlopeFactor = 0.0f;
    rasterizationState.depthBiasConstantFactor = 0.0f;
    rasterizationState.lineWidth = 1.0f;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.flags = 0;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.alphaToOneEnable = VK_FALSE;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.minSampleShading = 0.0f;
    multisampleState.pSampleMask = nullptr;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.blendEnable = VK_FALSE;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
    | VK_COLOR_COMPONENT_G_BIT
    | VK_COLOR_COMPONENT_B_BIT
    | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlendState.flags = 0;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &blendAttachment;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendState.blendConstants[0] = 0.0f;
    colorBlendState.blendConstants[1] = 0.0f;
    colorBlendState.blendConstants[2] = 0.0f;
    colorBlendState.blendConstants[3] = 0.0f;

    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.flags = 0;
    dynamicState.dynamicStateCount = 0;
    dynamicState.pDynamicStates = nullptr;

    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.stageCount = static_cast<U32>(stages.size());
    createInfo.pStages = stages.data();
    createInfo.pVertexInputState = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewportState;
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDepthStencilState = nullptr;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &dynamicState;
    createInfo.layout = layout.handle();
    createInfo.renderPass = renderPass.handle();
    createInfo.subpass = 0;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;

    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline));

    const std::vector<VkDescriptorSetLayout>& setLayouts = layout.descriptorSetLayouts();
    m_descriptorSets.resize(setLayouts.size(), VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    descriptorSetAllocateInfo.descriptorPool = descriptorPool.handle();
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<U32>(setLayouts.size());
    descriptorSetAllocateInfo.pSetLayouts = setLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, m_descriptorSets.data()));
}

ComputePipeline::ComputePipeline(
    VkDevice device,
    const DescriptorPool& descriptorPool,
    const PipelineLayout& layout,
    const Shader* shader
)
    :
    IPipeline(device)
{
    VkPipelineShaderStageCreateInfo shaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    shaderStage.flags = 0;
    shaderStage.stage = shader->stage();
    shaderStage.module = shader->handle();
    shaderStage.pName = "main";
    shaderStage.pSpecializationInfo = nullptr;

    VkComputePipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.stage = shaderStage;
    createInfo.layout = layout.handle();
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;

    VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline));

    const std::vector<VkDescriptorSetLayout>& setLayouts = layout.descriptorSetLayouts();
    m_descriptorSets.resize(setLayouts.size(), VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    descriptorSetAllocateInfo.descriptorPool = descriptorPool.handle();
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<U32>(setLayouts.size());
    descriptorSetAllocateInfo.pSetLayouts = setLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, m_descriptorSets.data()));
}
