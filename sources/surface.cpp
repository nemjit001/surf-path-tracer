#include "surface.h"

#include <cassert>
#include <cstring>

#include "surf.h"
#include "types.h"

Surface::Surface(U32 width, U32 height)
	:
	width(width),
	height(height),
	pixels(nullptr)
{
	pixels = static_cast<U32*>(MALLOC64(width * height * sizeof(U32)));
}

Surface::~Surface()
{
	FREE64(pixels);
}

Surface::Surface(Surface& other)
	:
	width(other.width),
	height(other.height),
	pixels(nullptr)
{
	pixels = static_cast<U32*>(MALLOC64(width * height * sizeof(U32)));
	assert(pixels != nullptr);
	memcpy(pixels, other.pixels, width * height * sizeof(U32));
}

Surface& Surface::operator=(Surface& other)
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
