#include "vk_layer/buffer.h"

#include <cassert>
#include <cstring>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/vk_check.h"

Buffer::Buffer(
    VmaAllocator allocator,
    SizeType size,
    VkBufferUsageFlags bufferUsage,
    VkMemoryPropertyFlags memoryProperties,
    VmaAllocationCreateFlags allocationFlags,
    VmaMemoryUsage memoryUsage
)
    :
    m_allocator(allocator),
    m_allocation(VK_NULL_HANDLE),
    m_allocationInfo{},
    m_buffer(VK_NULL_HANDLE)
{
    // Still allocate buffer if empty -> buggy hack to avoid stupid empty buffer logic everywhere else
    // FIXME: this is a VERY possible source of bugs
    if (size == 0)
        size = sizeof(U32);

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.queueFamilyIndexCount = 0;
    bufferCreateInfo.pQueueFamilyIndices = nullptr;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = bufferUsage;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.flags = allocationFlags;
    allocationCreateInfo.memoryTypeBits = 0;
    allocationCreateInfo.pool = VK_NULL_HANDLE;
    allocationCreateInfo.preferredFlags = 0;
    allocationCreateInfo.requiredFlags = memoryProperties;
    allocationCreateInfo.priority = 0.0f;
    allocationCreateInfo.pUserData = nullptr;
    allocationCreateInfo.usage = memoryUsage;

    VK_CHECK(vmaCreateBuffer(
        m_allocator,
        &bufferCreateInfo,
        &allocationCreateInfo,
        &m_buffer,
        &m_allocation,
        &m_allocationInfo
    ));
}

Buffer::~Buffer()
{
    release();
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

    this->release();
    this->m_allocator = other.m_allocator;
    this->m_allocation = other.m_allocation;
    this->m_allocationInfo = other.m_allocationInfo;
    this->m_buffer = other.m_buffer;
    
    other.m_allocation = VK_NULL_HANDLE;
    other.m_buffer = VK_NULL_HANDLE;

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

void Buffer::clear()
{
    void* pBufferData = nullptr;
    vmaMapMemory(m_allocator, m_allocation, &pBufferData);
    assert(pBufferData != nullptr);

    memset(pBufferData, 0, m_allocationInfo.size);
    vmaUnmapMemory(m_allocator, m_allocation);
}

void Buffer::persistentMap(void** ppData)
{
    void* pBufferData = nullptr;
    vmaMapMemory(m_allocator, m_allocation, &pBufferData);
    assert(pBufferData != nullptr);
    *ppData = pBufferData;
}

void Buffer::unmap()
{
    vmaUnmapMemory(m_allocator, m_allocation);
}

VkBuffer Buffer::handle() const
{
    return m_buffer;
}

void Buffer::release()
{
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}
