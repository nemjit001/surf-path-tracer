#pragma once

#include <string>
#include <vulkan/vulkan.h>

enum class ShaderType
{
    Vertex,
    Fragment,
    Compute,
    Undefined
};

class Shader
{
public:
    Shader();

    void initFromFile(VkDevice device, ShaderType type, const std::string& path);

    void destroy();

    VkShaderStageFlagBits stage() const;

    VkShaderModule handle() const;

private:
    VkDevice m_device;
    ShaderType m_type;
    VkShaderModule m_shader;
};
