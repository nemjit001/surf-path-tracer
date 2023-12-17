#pragma once

#include "types.h"

class Surface
{
public:
	Surface(U32 width, U32 height);

	~Surface();

	Surface(Surface& other);
	Surface& operator=(Surface& other);

public:
	U32 width;
	U32 height;
	U32* pixels;
};
