#pragma once

#include <stdint.h>
/// Disable dithering
#define DISABLE_DITHER 0x00
/// Enable dithering using binary dithering (only option)
#define BINARY_DITHER 0x01
/// The dither setting, either DISABLE_DITHER or BINARY_DITHER
typedef uint8_t EDitherMode;
