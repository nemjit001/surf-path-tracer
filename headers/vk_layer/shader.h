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

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other);
    Shader& operator=(Shader&& other);

    VkShaderStageFlagBits stage() const;

    VkShaderModule handle() const;

private:
    VkDevice m_device;
    ShaderType m_type;
    VkShaderModule m_shader;
};
