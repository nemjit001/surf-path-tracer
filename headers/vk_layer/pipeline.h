#pragma once

#include <vulkan/vulkan.h>

#include "types.h"

struct Viewport
{
    I32 x;
    I32 y;
    U32 width;
    U32 height;
    F32 minDepth;
    F32 maxDepth;
};

class PipelineLayout
{
public:
    PipelineLayout();

    void init(VkDevice device);

    void destroy();

    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;

    PipelineLayout(PipelineLayout&& other);
    PipelineLayout& operator=(PipelineLayout&& other);

    VkPipelineLayout handle() const;

private:
    VkDevice m_device;
    VkPipelineLayout m_layout;
};

class GraphicsPipeline
{
public:
    GraphicsPipeline();

    void init(VkDevice device, Viewport viewport, const PipelineLayout& layout);

    void destroy();

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    GraphicsPipeline(GraphicsPipeline&& other);
    GraphicsPipeline& operator=(GraphicsPipeline&& other);

private:
    VkDevice m_device;
    VkPipeline m_pipeline;
};
