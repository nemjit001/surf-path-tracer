#pragma once

#include <cstdlib>
#include <vulkan/vulkan.h>

#define VK_CHECK(result) (                  \
    (result == VK_SUCCESS) || (abort(), 0)  \
)
