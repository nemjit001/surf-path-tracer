#include "vk_layer/buffer.h"

#include <cassert>
#include <cstring>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"

Buffer::Buffer(VmaAllocator allocator, SizeType size, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage)
    :
    m_allocator(allocator),
    m_allocation(VK_NULL_HANDLE),
    m_allocationInfo{},
    m_buffer(VK_NULL_HANDLE)
{
    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.queueFamilyIndexCount = 0;
    bufferCreateInfo.pQueueFamilyIndices = nullptr;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = bufferUsage;

    // FIXME: Check if memory alloc works this way
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.flags = 0;
    allocationCreateInfo.memoryTypeBits = 0;
    allocationCreateInfo.pool = VK_NULL_HANDLE;
    allocationCreateInfo.preferredFlags = 0;
    allocationCreateInfo.requiredFlags = 0;
    allocationCreateInfo.priority = 0.0f;
    allocationCreateInfo.pUserData = nullptr;
    allocationCreateInfo.usage = memoryUsage;

    vmaCreateBuffer(
        allocator,
        &bufferCreateInfo,
        &allocationCreateInfo,
        &m_buffer,
        &m_allocation,
        &m_allocationInfo
    );
}

Buffer::~Buffer()
{
    destroy();
}

Buffer::Buffer(Buffer&& other)
    :
    m_allocator(other.m_allocator),
    m_allocation(other.m_allocation),
    m_allocationInfo(other.m_allocationInfo),
    m_buffer(other.m_buffer)
{
    other.m_allocation = VK_NULL_HANDLE;
    other.m_buffer = VK_NULL_HANDLE;
}

Buffer& Buffer::operator=(Buffer&& other)
{
    if (this == &other)
    {
        return *this;
    }

    this->destroy();
    this->m_allocator = other.m_allocator;
    this->m_allocation = other.m_allocation;
    this->m_allocationInfo = other.m_allocationInfo;
    this->m_buffer = other.m_buffer;

    return *this;
}

void Buffer::copyToBuffer(SizeType size, const void* data)
{
    assert(size <= m_allocationInfo.size);

    void* pBufferData = nullptr;
    vmaMapMemory(m_allocator, m_allocation, &pBufferData);
    assert(pBufferData != nullptr);

    memcpy(pBufferData, data, size);
    vmaUnmapMemory(m_allocator, m_allocation);
}

void Buffer::destroy()
{
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}
