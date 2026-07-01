/// @file wave3.cpp.hpp
/// @brief Out-of-line definitions for wave3 transposition internals.
///
/// Split from the former force-inline `detail/wave3.hpp` header so callers
/// that don't need per-TU inlining pay the code size once (link-time), not
/// once per translation unit. See `detail/wave3.h` for the public
/// declarations.

// IWYU pragma: private

#include "fl/channels/detail/wave3.h"
#include "fl/channels/detail/bit_spread_lut.hpp"
#include "fl/stl/compiler_control.h"
#include "fl/stl/isr/memcpy.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

namespace detail {

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_convert_byte_to_wave3byte(u8 byte_value,
                                      const Wave3BitExpansionLut& lut,
                                      Wave3Byte* output) FL_NO_EXCEPT {
    u16 high_pattern = lut.lut[(byte_value >> 4) & 0xF];
    u16 low_pattern = lut.lut[byte_value & 0xF];

    output->data[0] = (u8)(high_pattern >> 4);
    output->data[1] = (u8)(((high_pattern & 0xF) << 4) | ((low_pattern >> 8) & 0xF));
    output->data[2] = (u8)(low_pattern & 0xFF);
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_2(const Wave3Byte lane_waves[2],
                       u8 output[2 * sizeof(Wave3Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 l0 = lane_waves[0].data[symbol_idx];
        u8 l1 = lane_waves[1].data[symbol_idx];

        u16 interleaved = 0;
        for (int bit = 0; bit < 8; bit++) {
            u16 b0 = (l0 >> bit) & 1;
            u16 b1 = (l1 >> bit) & 1;
            interleaved |= (b1 << (bit * 2));
            interleaved |= (b0 << (bit * 2 + 1));
        }

        output[symbol_idx * 2] = (u8)(interleaved >> 8);
        output[symbol_idx * 2 + 1] = (u8)(interleaved & 0xFF);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_4(const Wave3Byte lane_waves[4],
                       u8 output[4 * sizeof(Wave3Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 l0 = lane_waves[0].data[symbol_idx];
        u8 l1 = lane_waves[1].data[symbol_idx];
        u8 l2 = lane_waves[2].data[symbol_idx];
        u8 l3 = lane_waves[3].data[symbol_idx];

        output[symbol_idx * 4 + 0] =
            ((l3 >> 7) & 1) << 7 |
            ((l2 >> 7) & 1) << 6 |
            ((l1 >> 7) & 1) << 5 |
            ((l0 >> 7) & 1) << 4 |
            ((l3 >> 6) & 1) << 3 |
            ((l2 >> 6) & 1) << 2 |
            ((l1 >> 6) & 1) << 1 |
            ((l0 >> 6) & 1);

        output[symbol_idx * 4 + 1] =
            ((l3 >> 5) & 1) << 7 |
            ((l2 >> 5) & 1) << 6 |
            ((l1 >> 5) & 1) << 5 |
            ((l0 >> 5) & 1) << 4 |
            ((l3 >> 4) & 1) << 3 |
            ((l2 >> 4) & 1) << 2 |
            ((l1 >> 4) & 1) << 1 |
            ((l0 >> 4) & 1);

        output[symbol_idx * 4 + 2] =
            ((l3 >> 3) & 1) << 7 |
            ((l2 >> 3) & 1) << 6 |
            ((l1 >> 3) & 1) << 5 |
            ((l0 >> 3) & 1) << 4 |
            ((l3 >> 2) & 1) << 3 |
            ((l2 >> 2) & 1) << 2 |
            ((l1 >> 2) & 1) << 1 |
            ((l0 >> 2) & 1);

        output[symbol_idx * 4 + 3] =
            ((l3 >> 1) & 1) << 7 |
            ((l2 >> 1) & 1) << 6 |
            ((l1 >> 1) & 1) << 5 |
            ((l0 >> 1) & 1) << 4 |
            ((l3 >> 0) & 1) << 3 |
            ((l2 >> 0) & 1) << 2 |
            ((l1 >> 0) & 1) << 1 |
            ((l0 >> 0) & 1);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_8(const Wave3Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave3Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 l[8];
        for (int lane = 0; lane < 8; lane++) {
            l[lane] = lane_waves[lane].data[symbol_idx];
        }
        spread_transpose8_symbol(l, output + symbol_idx * 8);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_16(const Wave3Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave3Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 l[16];
        for (int lane = 0; lane < 16; lane++) {
            l[lane] = lane_waves[lane].data[symbol_idx];
        }
        spread_transpose16_symbol(l, output + symbol_idx * 16);
    }
}

} // namespace detail

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3(u8 lane,
           const Wave3BitExpansionLut& lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave3Byte)]) FL_NO_EXCEPT {
    Wave3Byte wave3_output;
    detail::wave3_convert_byte_to_wave3byte(lane, lut, &wave3_output);
    output[0] = wave3_output.data[0];
    output[1] = wave3_output.data[1];
    output[2] = wave3_output.data[2];
}

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
