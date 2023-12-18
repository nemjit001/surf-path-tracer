#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"

class Buffer
{
public:
    Buffer(
        VmaAllocator allocator,
        SizeType size,
        VkBufferUsageFlags bufferUsage,
        VkMemoryPropertyFlags memoryProperties,
        VmaAllocationCreateFlags allocationFlags,
        VmaMemoryUsage memoryUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO
    );

    virtual ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    void copyToBuffer(SizeType size, const void* data);

private:
    void release();

private:
    VmaAllocator m_allocator;
    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocationInfo;
    VkBuffer m_buffer;
};
