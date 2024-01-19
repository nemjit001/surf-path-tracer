#include "vk_layer/framebuffer.h"

#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/vk_check.h"

Framebuffer::Framebuffer(
    VkDevice device,
    const RenderPass& renderPass,
    const std::vector<VkImageView>& attachments,
    U32 width,
    U32 height,
    U32 layers
)
    :
    m_device(device),
    m_framebuffer(VK_NULL_HANDLE)
{
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

Framebuffer::~Framebuffer()
{
    release();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    :
    m_device(other.m_device),
    m_framebuffer(other.m_framebuffer)
{
    other.m_framebuffer = VK_NULL_HANDLE;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_device = other.m_device;
    this->m_framebuffer = other.m_framebuffer;

    other.m_framebuffer = VK_NULL_HANDLE;

    return *this;
}

VkFramebuffer Framebuffer::handle() const
{
    return m_framebuffer;
}

void Framebuffer::release()
{
    vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
}
