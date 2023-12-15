#include "vk_layer/pipeline.h"

#include <vulkan/vulkan.h>

#include "vk_layer/vk_check.h"

GraphicsPipeline::GraphicsPipeline(VkDevice device)
    :
    m_device(device),
    m_pipeline(VK_NULL_HANDLE)
{
    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    createInfo.flags = 0;   // TODO: set gfx pipeline info

    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_pipeline));
}

GraphicsPipeline::~GraphicsPipeline()
{
    destroy();
}



void GraphicsPipeline::destroy()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
}
