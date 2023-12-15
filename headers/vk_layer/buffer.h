#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"

class Buffer
{
public:
    Buffer(VmaAllocator allocator, SizeType size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    virtual ~Buffer();

    // Copies disallowed to avoid accidental resource release
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other);
    Buffer& operator=(Buffer&& other);

    void copyToBuffer(SizeType size, const void* data);

private:
    void destroy();

private:
    VmaAllocator m_allocator;
    VmaAllocation m_allocation;
    VmaAllocationInfo m_allocationInfo;
    VkBuffer m_buffer;
};
