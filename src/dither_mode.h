/// @file dither_mode.h
/// Declares dithering options and types

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/int.h"
#include "fl/system/sketch_macros.h"
#if !FL_PLATFORM_HAS_TINY_MEMORY
#include "fl/channels/dither_frame.h"
#endif

/// Disable dithering
#define DISABLE_DITHER 0x00
/// Enable dithering using binary dithering (only option)
#define BINARY_DITHER 0x01
/// The dither setting, either DISABLE_DITHER or BINARY_DITHER

typedef fl::u8 EDitherMode;
