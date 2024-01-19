#include "vk_layer/sampler.h"

#include <vulkan/vulkan.h>

#include "vk_layer/vk_check.h"

Sampler::Sampler(VkDevice device)
	:
	m_device(device),
	m_sampler(VK_NULL_HANDLE)
{
	// FIXME: expose parts of sampler that need configuration in constructor call
	VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_sampler));
}

Sampler::~Sampler()
{
	release();
}

Sampler::Sampler(Sampler&& other) noexcept
	:
	m_device(other.m_device),
	m_sampler(other.m_sampler)
{
	other.m_sampler = VK_NULL_HANDLE;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	this->release();
	this->m_device = other.m_device;
	this->m_sampler = other.m_sampler;

	other.m_sampler = VK_NULL_HANDLE;

	return *this;
}

VkSampler Sampler::handle() const
{
	return m_sampler;
}

void Sampler::release()
{
	vkDestroySampler(m_device, m_sampler, nullptr);
}
