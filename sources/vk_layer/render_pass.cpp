#include "vk_layer/render_pass.h"

#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/vk_check.h"

RenderPass::RenderPass(VkDevice device, VkFormat colorFormat)
    :
    m_device(device),
    m_renderPass(VK_NULL_HANDLE)
{
    VkAttachmentDescription swapAttachment = {};
    swapAttachment.flags = 0;
    swapAttachment.format = colorFormat;
    swapAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    swapAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentReference swapAttachmentRef = {};
    swapAttachmentRef.attachment = 0;
    swapAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &swapAttachmentRef;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &swapAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 0;
    createInfo.pDependencies = nullptr;

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
