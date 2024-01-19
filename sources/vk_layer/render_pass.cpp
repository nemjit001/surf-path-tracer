#include "vk_layer/render_pass.h"

#include <vulkan/vulkan.h>

#include "surf.h"
#include "types.h"
#include "vk_layer/vk_check.h"

RenderPass::RenderPass(
    VkDevice device,
    std::vector<ImageAttachment> attachments,
    std::vector<AttachmentReference> attachmentRefs
)
    :
    m_device(device),
    m_renderPass(VK_NULL_HANDLE)
{
    std::vector<VkAttachmentDescription> vkAttachments;
    std::vector<VkAttachmentReference> vkColorAttachmentRefs;

    for (const auto& attachment : attachments)
    {
        vkAttachments.push_back(VkAttachmentDescription{
            0, attachment.format, attachment.sampleCount,
            attachment.imageOps.load, attachment.imageOps.store,
            attachment.stencilOps.load, attachment.stencilOps.store,
            attachment.initialLayout, attachment.finalLayout
        });
    }
    
    for (const auto& ref : attachmentRefs)
    {
        switch (ref.type)
        {
        case AttachmentType::Color:
            vkColorAttachmentRefs.push_back(VkAttachmentReference{ ref.attachment, ref.layout });
            break;
        case AttachmentType::Resolve:
        case AttachmentType::DepthStencil:
        case AttachmentType::Input:
        case AttachmentType::Preserve:
            FATAL_ERROR("Attachment ref types other than AttachmentType::Color NYI");
            break;
        }
    }

    VkSubpassDescription subpass = {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<U32>(vkColorAttachmentRefs.size());
    subpass.pColorAttachments = vkColorAttachmentRefs.data();
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkSubpassDependency prevFrameDependency = {};
    prevFrameDependency.dependencyFlags = 0;
    prevFrameDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    prevFrameDependency.dstSubpass = 0;
    prevFrameDependency.srcAccessMask = 0;
    prevFrameDependency.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    prevFrameDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    prevFrameDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.attachmentCount = static_cast<U32>(vkAttachments.size());
    createInfo.pAttachments = vkAttachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &prevFrameDependency;

    VK_CHECK(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass));
}

RenderPass::~RenderPass()
{
    release();
}

RenderPass::RenderPass(RenderPass&& other) noexcept
    :
    m_device(other.m_device),
    m_renderPass(other.m_renderPass)
{
    other.m_renderPass = VK_NULL_HANDLE;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_device = other.m_device;
    this->m_renderPass = other.m_renderPass;

    other.m_renderPass = VK_NULL_HANDLE;

    return *this;
}

VkRenderPass RenderPass::handle() const
{
    return m_renderPass;
}

void RenderPass::release()
{
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
}
