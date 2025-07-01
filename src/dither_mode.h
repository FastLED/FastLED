/// @file dither_mode.h
/// Declares dithering options and types

#pragma once

#include "fl/stdint.h"
#include "fl/int.h"

#include "fl/namespace.h"

/// Disable dithering
#define DISABLE_DITHER 0x00
/// Enable dithering using binary dithering (only option)
#define BINARY_DITHER 0x01
/// The dither setting, either DISABLE_DITHER or BINARY_DITHER
FASTLED_NAMESPACE_BEGIN
typedef fl::u8 EDitherMode;
FASTLED_NAMESPACE_END
