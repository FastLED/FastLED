/// @file brightness_bitshifter.h
/// Defines brightness bitshifting functions

#pragma once

#include <stdint.h>

#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

uint8_t brightness_bitshifter8(uint8_t *brightness_src, uint8_t *brightness_dst, uint8_t max_shifts);

// Return value is the number of shifts on the src. Multiply this by the number of steps to get the
// the number of shifts on the dst.
uint8_t brightness_bitshifter16(uint8_t *brightness_src, uint16_t *brightness_dst, uint8_t max_shifts, uint8_t steps=2);

FASTLED_NAMESPACE_END