#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/render_pass.h"

class Framebuffer
{
public:
    Framebuffer();

    void init(
        VkDevice device,
        const RenderPass& renderPass,
        const std::vector<VkImageView>& attachments,
        U32 width,
        U32 height,
        U32 layers = 1
    );

    void destroy();

    VkFramebuffer handle() const;

private:
    VkDevice m_device;
    VkFramebuffer m_framebuffer;
};
