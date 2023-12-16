#pragma once

#include <vulkan/vulkan.h>

class RenderPass
{
public:
    RenderPass();

    void init(VkDevice device, VkFormat colorFormat);

    void destroy();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    RenderPass(RenderPass&& other);
    RenderPass& operator=(RenderPass&& other);

    VkRenderPass handle() const;

private:
    VkDevice m_device;
    VkRenderPass m_renderPass;
};
