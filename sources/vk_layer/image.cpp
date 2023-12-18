#include "vk_layer/image.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/vk_check.h"

Image::Image(
    VmaAllocator allocator,
    VkFormat imageFormat,
    U32 width,
    U32 height,
    VkImageUsageFlags imageUsage,
    VmaMemoryUsage memoryUsage
)
    :
    m_allocator(allocator),
    m_allocation(VK_NULL_HANDLE),
    m_allocationInfo{},
    m_image(VK_NULL_HANDLE)
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
}

Image::~Image()
{
    release();
}

Image::Image(Image&& other)
    :
    m_allocator(other.m_allocator),
    m_allocation(other.m_allocation),
    m_allocationInfo(other.m_allocationInfo),
    m_image(other.m_image)
{
    other.m_allocation = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other)
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_allocator = other.m_allocator;
    this->m_allocation = other.m_allocation;
    this->m_allocationInfo = other.m_allocationInfo;
    this->m_image = other.m_image;

    return *this;
}

void Image::release()
{
    vmaDestroyImage(m_allocator, m_image, m_allocation);
}
