#pragma once

#include <string>
#include <vulkan/vulkan.h>

enum class ShaderType
{
    Vertex,
    Fragment,
    Compute,
};

class Shader
{
public:
    Shader(VkDevice device, ShaderType type, const std::string& path);

    ~Shader();

    Shader(Shader&) = delete;
    Shader& operator=(Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    VkShaderStageFlagBits stage() const;

    VkShaderModule handle() const;

private:
    void release();

private:
    VkDevice m_device;
    ShaderType m_type;
    VkShaderModule m_shader;
};
