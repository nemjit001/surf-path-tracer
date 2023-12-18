#include "vk_layer/image.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/vk_check.h"

Image::Image(
    VkDevice device,
    VmaAllocator allocator,
    VkFormat imageFormat,
    U32 width,
    U32 height,
    VkImageUsageFlags imageUsage,
    VmaMemoryUsage memoryUsage
)
    :
    m_device(device),
    m_allocator(allocator),
    m_allocation(VK_NULL_HANDLE),
    m_allocationInfo{},
    m_image(VK_NULL_HANDLE),
    m_view(VK_NULL_HANDLE)
{
    VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = imageFormat;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = imageUsage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.flags = 0;
    allocationCreateInfo.memoryTypeBits = 0;
    allocationCreateInfo.pool = VK_NULL_HANDLE;
    allocationCreateInfo.preferredFlags = 0;
    allocationCreateInfo.requiredFlags = 0;
    allocationCreateInfo.priority = 0.0f;
    allocationCreateInfo.pUserData = nullptr;
    allocationCreateInfo.usage = memoryUsage;

    VK_CHECK(vmaCreateImage(
        m_allocator,
        &imageCreateInfo,
        &allocationCreateInfo,
        &m_image,
        &m_allocation,
        &m_allocationInfo
    ));

    VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewCreateInfo.flags = 0;
    viewCreateInfo.image = m_image;
    viewCreateInfo.format = imageFormat;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.components = VkComponentMapping{
        VK_COMPONENT_SWIZZLE_IDENTITY,  // R
        VK_COMPONENT_SWIZZLE_IDENTITY,  // G
        VK_COMPONENT_SWIZZLE_IDENTITY,  // B
        VK_COMPONENT_SWIZZLE_IDENTITY   // A
    };
    viewCreateInfo.subresourceRange = VkImageSubresourceRange{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,   // 0th mip level, 1 level
        0, 1    // 0th array layer, 1 layer
    };

    VK_CHECK(vkCreateImageView(
        m_device,
        &viewCreateInfo,
        nullptr,
        &m_view
    ));
}

Image::~Image()
{
    release();
}

Image::Image(Image&& other) noexcept
    :
    m_device(other.m_device),
    m_allocator(other.m_allocator),
    m_allocation(other.m_allocation),
    m_allocationInfo(other.m_allocationInfo),
    m_image(other.m_image),
    m_view(other.m_view)
{
    other.m_allocation = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_view = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_device = other.m_device;
    this->m_allocator = other.m_allocator;
    this->m_allocation = other.m_allocation;
    this->m_allocationInfo = other.m_allocationInfo;
    this->m_image = other.m_image;
    this->m_view = other.m_view;

    return *this;
}

VkImageView Image::view() const
{
    return m_view;
}

void Image::release()
{
    vmaDestroyImage(m_allocator, m_image, m_allocation);
    vkDestroyImageView(m_device, m_view, nullptr);
}
