/// @file wave8.h
/// @brief Out-of-line declarations for wave8 transposition internals.
///
/// Split from the former `detail/wave8.hpp` force-inline header. Callers now
/// include this .h and link against `detail/wave8.cpp.hpp`. Constexpr LUTs
/// and the `FL_WAVE8_SPREAD_TO_16` macro stay in this .h because they need
/// to be visible at compile time; every function definition moves to the
/// .cpp.hpp for one-per-program-image emit rather than per-TU inlining.

#pragma once

// IWYU pragma: private

#include "fl/channels/wave8.h"
#include "fl/stl/compiler_control.h"

namespace fl {

namespace detail {

// ============================================================================
// Constexpr LUTs (header — compile-time visibility required)
// ============================================================================

/// 2-lane LUT: Spreads 4 bits into 2-lane interleaved positions (nibble → byte).
constexpr u8 kTranspose4_16_LUT[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11,
                                            0x14, 0x15, 0x40, 0x41, 0x44, 0x45,
                                            0x50, 0x51, 0x54, 0x55};

/// 4-lane LUT: Spreads 2 bits into 4-lane interleaved positions (2-bit → byte).
constexpr u8 kTranspose2_4_LUT[4] = {
    0x00,  // 0b00 → no bits set
    0x01,  // 0b01 → bit at position 0 (pulse 1)
    0x10,  // 0b10 → bit at position 4 (pulse 0)
    0x11   // 0b11 → bits at positions 0 and 4 (both pulses)
};

} // namespace detail
} // namespace fl

// ============================================================================
// 2-Lane Transposition Helper Macro (header — expanded at each use)
// ============================================================================

#define FL_WAVE8_SPREAD_TO_16(lane_u8_0, lane_u8_1, out_16)                            \
    do {                                                                       \
        const ::fl::u8 _a = (::fl::u8)(lane_u8_0);                               \
        const ::fl::u8 _b = (::fl::u8)(lane_u8_1);                               \
        const ::fl::u16 _even =                                                 \
            (::fl::u16)((::fl::u16)::fl::detail::kTranspose4_16_LUT[_b & 0x0Fu] |        \
                       ((::fl::u16)::fl::detail::kTranspose4_16_LUT[_b >> 4] << 8));    \
        const ::fl::u16 _odd =                                                  \
            (::fl::u16)(((::fl::u16)::fl::detail::kTranspose4_16_LUT[_a & 0x0Fu] |       \
                        ((::fl::u16)::fl::detail::kTranspose4_16_LUT[_a >> 4] << 8))    \
                       << 1);                                                  \
        (out_16) |= (::fl::u16)(_even | _odd);                                  \
    } while (0)

namespace fl {
namespace detail {

// ============================================================================
// Out-of-line function declarations
// ============================================================================

void wave8_convert_byte_to_wave8byte(u8 byte_value,
                                      const Wave8BitExpansionLut& lut,
                                      Wave8Byte* output) FL_NO_EXCEPT;

void wave8_expand_byte(u8 byte_value,
                       const Wave8ByteExpansionLut& lut,
                       Wave8Byte* output) FL_NO_EXCEPT;

void wave8_transpose_2(const Wave8Byte lane_waves[2],
                       u8 output[2 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_4(const Wave8Byte lane_waves[4],
                       u8 output[4 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_8(const Wave8Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_16(const Wave8Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_16x2_pipe2(const Wave8Byte lane_waves_a[16],
                                const Wave8Byte lane_waves_b[16],
                                u8 output_a[16 * sizeof(Wave8Byte)],
                                u8 output_b[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_16x4_pipe4(const Wave8Byte lane_waves_a[16],
                                const Wave8Byte lane_waves_b[16],
                                const Wave8Byte lane_waves_c[16],
                                const Wave8Byte lane_waves_d[16],
                                u8 output_a[16 * sizeof(Wave8Byte)],
                                u8 output_b[16 * sizeof(Wave8Byte)],
                                u8 output_c[16 * sizeof(Wave8Byte)],
                                u8 output_d[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_16_bf1(const u8 lanes[16],
                            u8 W0, u8 W1,
                            u8 output[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_8_bf1(const u8 lanes[8],
                           u8 W0, u8 W1,
                           u8 output[8 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_4_bf1(const u8 lanes[4],
                           u8 W0, u8 W1,
                           u8 output[4 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_2_bf1(const u8 lanes[2],
                           u8 W0, u8 W1,
                           u8 output[2 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

void wave8_transpose_16x4_bf1_pipe4(const u8 lanes_a[16],
                                    const u8 lanes_b[16],
                                    const u8 lanes_c[16],
                                    const u8 lanes_d[16],
                                    u8 W0, u8 W1,
                                    u8 output_a[16 * sizeof(Wave8Byte)],
                                    u8 output_b[16 * sizeof(Wave8Byte)],
                                    u8 output_c[16 * sizeof(Wave8Byte)],
                                    u8 output_d[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT;

} // namespace detail

/// @brief Public single-lane wave8 encoder. Thin wrapper around
///        `detail::wave8_convert_byte_to_wave8byte` + a memcpy — kept for
///        source-compat with existing callers.
void wave8(u8 lane,
           const Wave8BitExpansionLut& lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]) FL_NO_EXCEPT;

} // namespace fl
