#include "vk_layer/framebuffer.h"

#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/vk_check.h"

Framebuffer::Framebuffer()
    :
    m_device(VK_NULL_HANDLE),
    m_framebuffer(VK_NULL_HANDLE)
{
    //
}

void Framebuffer::init(
    VkDevice device,
    const RenderPass& renderPass,
    const std::vector<VkImageView>& attachments,
    U32 width,
    U32 height,
    U32 layers
)
{
    m_device = device;

    VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.renderPass = renderPass.handle();
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = layers;
    createInfo.attachmentCount = static_cast<U32>(attachments.size());
    createInfo.pAttachments = attachments.data();

    VK_CHECK(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffer));
}

void Framebuffer::destroy()
{
    vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
}

VkFramebuffer Framebuffer::handle() const
{
    return m_framebuffer;
}
