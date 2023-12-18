#include "pixel_buffer.h"

#include <cassert>
#include <cstring>

#include "surf.h"
#include "types.h"

PixelBuffer::PixelBuffer(U32 width, U32 height)
	:
	width(width),
	height(height),
	pixels(nullptr)
{
	pixels = static_cast<U32*>(MALLOC64(width * height * sizeof(U32)));
}

PixelBuffer::~PixelBuffer()
{
	FREE64(pixels);
}

PixelBuffer::PixelBuffer(PixelBuffer& other)
	:
	width(other.width),
	height(other.height),
	pixels(nullptr)
{
	pixels = static_cast<U32*>(MALLOC64(width * height * sizeof(U32)));
	assert(pixels != nullptr);
	memcpy(pixels, other.pixels, width * height * sizeof(U32));
}

PixelBuffer& PixelBuffer::operator=(PixelBuffer& other)
{
	if (this == &other)
	{
		return *this;
	}

	FREE64(this->pixels);
	this->width = other.width;
	this->height = other.height;
	this->pixels = static_cast<U32*>(MALLOC64(width * height * sizeof(U32)));
	assert(this->pixels != nullptr);
	memcpy(this->pixels, other.pixels, width * height * sizeof(U32));

	return *this;
}
