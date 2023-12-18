#pragma once

#include <vulkan/vulkan.h>

class DescriptorPool
{
public:
    DescriptorPool(VkDevice device);

    ~DescriptorPool();

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    DescriptorPool(DescriptorPool&& other);
    DescriptorPool& operator=(DescriptorPool&& other);

    VkDescriptorPool handle() const;

private:
    void release();

private:
    VkDevice m_device;
    VkDescriptorPool m_pool;
};
