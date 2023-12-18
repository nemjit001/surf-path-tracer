#pragma once

#include "types.h"

class PixelBuffer
{
public:
	PixelBuffer(U32 width, U32 height);

	~PixelBuffer();

	PixelBuffer(PixelBuffer& other);
	PixelBuffer& operator=(PixelBuffer& other);

public:
	U32 width;
	U32 height;
	U32* pixels;
};
