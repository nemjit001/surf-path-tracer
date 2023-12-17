#pragma once

#include <vulkan/vulkan.h>

// Surf main header file
#define PROGRAM_NAME	"Surf Path Tracer"
#define PROGRAM_VERSION VK_MAKE_API_VERSION(0, 0, 0, 0)
#define SCR_WIDTH		1280
#define SCR_HEIGHT		720

#define MALLOC64(size)  _aligned_malloc(size, 64)
#define FREE64(block)	_aligned_free(block)
