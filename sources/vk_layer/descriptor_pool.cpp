#include "vk_layer/descriptor_pool.h"

#include "vk_layer/vk_check.h"

#define MAX_DESCRIPTOR_SETS 256

DescriptorPool::DescriptorPool(VkDevice device)
    :
    m_device(device),
    m_pool(VK_NULL_HANDLE)
{
    static VkDescriptorPoolSize poolSizes[] = {
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_DESCRIPTOR_SETS },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_DESCRIPTOR_SETS },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_DESCRIPTOR_SETS },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MAX_DESCRIPTOR_SETS },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, MAX_DESCRIPTOR_SETS },
    };

    VkDescriptorPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    createInfo.maxSets = MAX_DESCRIPTOR_SETS;
    createInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
    createInfo.pPoolSizes = poolSizes;

    VK_CHECK(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_pool));
}

DescriptorPool::~DescriptorPool()
{
    release();
}

DescriptorPool::DescriptorPool(DescriptorPool&& other)
    :
    m_device(other.m_device),
    m_pool(other.m_pool)
{
    other.m_pool = VK_NULL_HANDLE;
}

DescriptorPool& DescriptorPool::operator=(DescriptorPool&& other)
{
    if (this == &other)
    {
        return *this;
    }

    this->release();
    this->m_device = other.m_device;
    this->m_pool = other.m_pool;

    return *this;
}

VkDescriptorPool DescriptorPool::handle() const
{
    return m_pool;
}

void DescriptorPool::release()
{
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
}
