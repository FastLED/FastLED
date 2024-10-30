#pragma once

#include <stdint.h>

#include "namespace.h"

/// Disable dithering
#define DISABLE_DITHER 0x00
/// Enable dithering using binary dithering (only option)
#define BINARY_DITHER 0x01
/// The dither setting, either DISABLE_DITHER or BINARY_DITHER
FASTLED_NAMESPACE_BEGIN
typedef uint8_t EDitherMode;
FASTLED_NAMESPACE_END