#include "vk_layer/shader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vulkan/vulkan.h>

#include "types.h"
#include "vk_layer/vk_check.h"

Shader::Shader()
    :
    m_device(VK_NULL_HANDLE),
    m_type(ShaderType::Undefined),
    m_shader(VK_NULL_HANDLE)
{
    //
}

void Shader::initFromFile(VkDevice device, ShaderType type, const std::string& path)
{
    m_device = device;
    m_type = type;

    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.good())
    {
        std::cerr << "Failed to open shader file: " << path << '\n';
        abort();
    }

    SizeType codeSize = file.tellg();
    file.seekg(std::ios::beg);

    char* byteCode = new char[codeSize + 1]{};
    file.read(byteCode, codeSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.codeSize = codeSize;
    createInfo.pCode = reinterpret_cast<U32*>(byteCode);

    VK_CHECK(vkCreateShaderModule(m_device, &createInfo, nullptr, &m_shader));
    delete[] byteCode;
}

void Shader::destroy()
{
    vkDestroyShaderModule(m_device, m_shader, nullptr);
}

Shader::Shader(Shader&& other)
    :
    m_device(other.m_device),
    m_type(other.m_type),
    m_shader(other.m_shader)
{
    other.m_type = ShaderType::Undefined;
    other.m_shader = VK_NULL_HANDLE;
}

Shader& Shader::operator=(Shader&& other)
{
    if (this == &other)
    {
        return *this;
    }

    this->destroy();
    this->m_device = other.m_device;
    this->m_type = other.m_type;
    this->m_shader = other.m_shader;

    return *this;
}

VkShaderStageFlagBits Shader::stage() const
{
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

    switch (m_type)
    {
    case ShaderType::Vertex:
        stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderType::Fragment:
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderType::Compute:
        stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;    
    default:
        break;
    }

    return stage;
}

VkShaderModule Shader::handle() const
{
    assert(m_type != ShaderType::Undefined);
    return m_shader;
}
