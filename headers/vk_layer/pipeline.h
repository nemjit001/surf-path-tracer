#pragma once

#include <vulkan/vulkan.h>

class GraphicsPipeline
{
public:
    GraphicsPipeline(VkDevice device);

    virtual ~GraphicsPipeline();

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    GraphicsPipeline(GraphicsPipeline&& other);
    GraphicsPipeline& operator=(GraphicsPipeline&& other);

private:
    void destroy();

private:
    VkDevice m_device;
    VkPipeline m_pipeline;
};
