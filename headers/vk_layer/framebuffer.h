#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/render_pass.h"

class Framebuffer
{
public:
    Framebuffer(
        VkDevice device,
        const RenderPass& renderPass,
        const std::vector<VkImageView>& attachments,
        U32 width,
        U32 height,
        U32 layers = 1
    );

    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    VkFramebuffer handle() const;

private:
    void release();

private:
    VkDevice m_device;
    VkFramebuffer m_framebuffer;
};
