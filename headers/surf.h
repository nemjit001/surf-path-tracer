#pragma once

#include <cstdio>
#include <cstdlib>
#include <vulkan/vulkan.h>

// Surf main header file
#define PROGRAM_NAME	"Surf Path Tracer"
#define PROGRAM_VERSION VK_MAKE_API_VERSION(0, 0, 0, 0)
#define SCR_WIDTH		1280
#define SCR_HEIGHT		720

#define FATAL_ERROR(msg) (										\
	(fprintf(stderr, "Surf Fatal Error: %s\n", #msg)), abort()	\
)

#if defined(_WIN32)

#define MALLOC64(size)  _aligned_malloc(size, 64)
#define FREE64(block)	_aligned_free(block)

#define ALIGN(alignment)	__declspec(align(alignment))

#elif defined(__unix)

#define MALLOC64(size)  aligned_alloc(64, size)
#define FREE64(block)	free(block)

#define ALIGN(alignment)	__attribute__((aligned(alignment)))

#endif

#if defined(_WIN32)
// Disable as much windows junk as possible

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NOIME

#include <windows.h>
#endif
