#pragma once

#include <vulkan/vulkan.h>

class RenderPass
{
public:
    RenderPass();

    void init(VkDevice device, VkFormat colorFormat);

    void destroy();

    VkRenderPass handle() const;

private:
    VkDevice m_device;
    VkRenderPass m_renderPass;
};
