/// @file wave3.h
/// @brief Out-of-line declarations for wave3 transposition internals.
///
/// Split from the former `detail/wave3.hpp` force-inline header. Callers now
/// include this .h and link against `detail/wave3.cpp.hpp`. The transposes
/// are still in IRAM and are still `FL_OPTIMIZE_FUNCTION` — they are just no
/// longer force-inlined into every translation unit that references them.
/// The unity build compiles the .cpp.hpp once and every caller resolves via
/// normal link.

#pragma once

// IWYU pragma: private

#include "fl/channels/wave3.h"
#include "fl/stl/compiler_control.h"

namespace fl {

namespace detail {

/// @brief Convert one byte to a `Wave3Byte` using the nibble LUT.
///        Split-nibble path (high nibble + low nibble); bit-identical output
///        to a wave3-eligible chipset's expected pulse pattern.
void wave3_convert_byte_to_wave3byte(u8 byte_value,
                                      const Wave3BitExpansionLut& lut,
                                      Wave3Byte* output) FL_NO_EXCEPT;

/// @brief Transpose 2 lanes of Wave3Byte data into interleaved format.
void wave3_transpose_2(const Wave3Byte lane_waves[2],
                       u8 output[2 * sizeof(Wave3Byte)]) FL_NO_EXCEPT;

/// @brief Transpose 4 lanes of Wave3Byte data.
void wave3_transpose_4(const Wave3Byte lane_waves[4],
                       u8 output[4 * sizeof(Wave3Byte)]) FL_NO_EXCEPT;

/// @brief Transpose 8 lanes of Wave3Byte data via the spread-LUT path (#2533).
void wave3_transpose_8(const Wave3Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave3Byte)]) FL_NO_EXCEPT;

/// @brief Transpose 16 lanes of Wave3Byte data via the spread-LUT path (#2533).
void wave3_transpose_16(const Wave3Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave3Byte)]) FL_NO_EXCEPT;

} // namespace detail

/// @brief Public single-lane wave3 encoder. Thin wrapper around
///        `detail::wave3_convert_byte_to_wave3byte` — kept for source-
///        compat with existing callers.
void wave3(u8 lane,
           const Wave3BitExpansionLut& lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave3Byte)]) FL_NO_EXCEPT;

} // namespace fl
