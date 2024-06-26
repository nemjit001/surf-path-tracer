#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"

class Image
{
public:
    Image(
        VkDevice device,
        VmaAllocator allocator,
        VkFormat imageFormat,
        U32 width,
        U32 height,
        VkImageUsageFlags imageUsage,
        VmaMemoryUsage memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO
    );

    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    VkImage handle() const;

    VkImageView view() const;

private:
    void release();

private:
    VkDevice m_device;
    VmaAllocator m_allocator;
    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocationInfo;
    VkImage m_image;
    VkImageView m_view;
};
