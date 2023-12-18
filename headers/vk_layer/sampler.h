#pragma once

#include <vulkan/vulkan.h>

class Sampler
{
public:
	Sampler(VkDevice device);

	~Sampler();

	Sampler(const Sampler&) = delete;
	Sampler& operator=(const Sampler&) = delete;

	Sampler(Sampler&& other) noexcept;
	Sampler& operator=(Sampler&& other) noexcept;

	VkSampler handle() const;

private:
	void release();

private:
	VkDevice m_device;
	VkSampler m_sampler;
};
