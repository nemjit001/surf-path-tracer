#pragma once

#include <vulkan/vulkan.h>

#include "render_context.h"

class RenderPass
{
public:
    RenderPass(VkDevice device, VkFormat colorFormat);

    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    RenderPass(RenderPass&& other) noexcept;
    RenderPass& operator=(RenderPass&& other) noexcept;

    VkRenderPass handle() const;

private:
    void release();

private:
    VkDevice m_device;
    VkRenderPass m_renderPass;
};
