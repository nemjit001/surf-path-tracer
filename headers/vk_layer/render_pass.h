#pragma once

#include <vulkan/vulkan.h>

#include "render_context.h"

enum class AttachmentType
{
    Color,
    Resolve,
    DepthStencil,
    Input,
    Preserve
};

struct ImageOps
{
    VkAttachmentLoadOp load;
    VkAttachmentStoreOp store;
};

struct ImageAttachment
{
    VkFormat format;
    VkSampleCountFlagBits sampleCount;
    ImageOps imageOps;
    ImageOps stencilOps;
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
};

struct AttachmentReference
{
    AttachmentType type;
    U32 attachment;
    VkImageLayout layout;
};

class RenderPass
{
public:
    RenderPass(
        VkDevice device,
        std::vector<ImageAttachment> attachments,
        std::vector<AttachmentReference> attachmentRefs
    );

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
