/// @file wave8.cpp.hpp
/// @brief Out-of-line definitions for wave8 transposition internals.
///
/// Split from the former force-inline `detail/wave8.hpp` header so callers
/// that don't need per-TU inlining pay the code size once (link-time), not
/// once per translation unit. See `detail/wave8.h` for the public
/// declarations.

// IWYU pragma: private

#include "fl/channels/detail/wave8.h"
#include "fl/channels/detail/bit_spread_lut.hpp"
#include "fl/stl/bit_cast.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/isr/memcpy.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

namespace detail {

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_convert_byte_to_wave8byte(u8 byte_value,
                                      const Wave8BitExpansionLut& lut,
                                      Wave8Byte* output) FL_NO_EXCEPT {
    const Wave8Bit* high_nibble_data = lut.lut[(byte_value >> 4) & 0xF];
    isr::memcpy_32(fl::bit_cast_ptr<u32>(&output->symbols[0]),
                  fl::bit_cast_ptr<const u32>(high_nibble_data),
                  1);

    const Wave8Bit* low_nibble_data = lut.lut[byte_value & 0xF];
    isr::memcpy_32(fl::bit_cast_ptr<u32>(&output->symbols[4]),
                  fl::bit_cast_ptr<const u32>(low_nibble_data),
                  1);
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_expand_byte(u8 byte_value,
                       const Wave8ByteExpansionLut& lut,
                       Wave8Byte* output) FL_NO_EXCEPT {
    isr::memcpy_32(fl::bit_cast_ptr<u32>(output),
                  fl::bit_cast_ptr<const u32>(&lut.lut[byte_value]),
                  2);
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_2(const Wave8Byte lane_waves[2],
                       u8 output[2 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u16 interleaved = 0;
        FL_WAVE8_SPREAD_TO_16(lane_waves[0].symbols[symbol_idx].data,
                              lane_waves[1].symbols[symbol_idx].data,
                              interleaved);

        output[symbol_idx * 2] = (u8)(interleaved >> 8);
        output[symbol_idx * 2 + 1] = (u8)(interleaved & 0xFF);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_4(const Wave8Byte lane_waves[4],
                       u8 output[4 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 l0 = lane_waves[0].symbols[symbol_idx].data;
        u8 l1 = lane_waves[1].symbols[symbol_idx].data;
        u8 l2 = lane_waves[2].symbols[symbol_idx].data;
        u8 l3 = lane_waves[3].symbols[symbol_idx].data;

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
void wave8_transpose_8(const Wave8Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 l[8];
        for (int lane = 0; lane < 8; lane++) {
            l[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }
        spread_transpose8_symbol(l, output + symbol_idx * 8);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16(const Wave8Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 l[16];
        for (int lane = 0; lane < 16; lane++) {
            l[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }
        spread_transpose16_symbol(l, output + symbol_idx * 16);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16x2_pipe2(const Wave8Byte lane_waves_a[16],
                                const Wave8Byte lane_waves_b[16],
                                u8 output_a[16 * sizeof(Wave8Byte)],
                                u8 output_b[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 la[16];
        u8 lb[16];
        for (int lane = 0; lane < 16; lane++) {
            la[lane] = lane_waves_a[lane].symbols[symbol_idx].data;
            lb[lane] = lane_waves_b[lane].symbols[symbol_idx].data;
        }
        spread_transpose16_symbol(la, output_a + symbol_idx * 16);
        spread_transpose16_symbol(lb, output_b + symbol_idx * 16);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16x4_pipe4(const Wave8Byte lane_waves_a[16],
                                const Wave8Byte lane_waves_b[16],
                                const Wave8Byte lane_waves_c[16],
                                const Wave8Byte lane_waves_d[16],
                                u8 output_a[16 * sizeof(Wave8Byte)],
                                u8 output_b[16 * sizeof(Wave8Byte)],
                                u8 output_c[16 * sizeof(Wave8Byte)],
                                u8 output_d[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 la[16];
        u8 lb[16];
        u8 lc[16];
        u8 ld[16];
        for (int lane = 0; lane < 16; lane++) {
            la[lane] = lane_waves_a[lane].symbols[symbol_idx].data;
            lb[lane] = lane_waves_b[lane].symbols[symbol_idx].data;
            lc[lane] = lane_waves_c[lane].symbols[symbol_idx].data;
            ld[lane] = lane_waves_d[lane].symbols[symbol_idx].data;
        }
        spread_transpose16_symbol(la, output_a + symbol_idx * 16);
        spread_transpose16_symbol(lb, output_b + symbol_idx * 16);
        spread_transpose16_symbol(lc, output_c + symbol_idx * 16);
        spread_transpose16_symbol(ld, output_d + symbol_idx * 16);
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16_bf1(const u8 lanes[16],
                            u8 W0, u8 W1,
                            u8 output[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    u8 d_mask[8];
    u8 m0_mask[8];
    const u8 D_byte = W0 ^ W1;
    for (int p = 0; p < 8; ++p) {
        const int shift = 7 - p;
        d_mask[p] = ((D_byte >> shift) & 1) ? 0xFFu : 0x00u;
        m0_mask[p] = ((W0 >> shift) & 1) ? 0xFFu : 0x00u;
    }
    u8 cols[16];
    spread_transpose16_symbol(lanes, cols);
    for (int s = 0; s < 8; ++s) {
        const u8 col_lo = cols[2 * s + 0];
        const u8 col_hi = cols[2 * s + 1];
        for (int p = 0; p < 8; ++p) {
            output[s * 16 + p * 2 + 0] = m0_mask[p] ^ (col_lo & d_mask[p]);
            output[s * 16 + p * 2 + 1] = m0_mask[p] ^ (col_hi & d_mask[p]);
        }
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_8_bf1(const u8 lanes[8],
                           u8 W0, u8 W1,
                           u8 output[8 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    u8 d_mask[8];
    u8 m0_mask[8];
    const u8 D_byte = W0 ^ W1;
    for (int p = 0; p < 8; ++p) {
        const int shift = 7 - p;
        d_mask[p] = ((D_byte >> shift) & 1) ? 0xFFu : 0x00u;
        m0_mask[p] = ((W0 >> shift) & 1) ? 0xFFu : 0x00u;
    }
    u8 cols[8];
    spread_transpose8_symbol(lanes, cols);
    for (int s = 0; s < 8; ++s) {
        const u8 col = cols[s];
        for (int p = 0; p < 8; ++p) {
            output[s * 8 + p] = m0_mask[p] ^ (col & d_mask[p]);
        }
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_4_bf1(const u8 lanes[4],
                           u8 W0, u8 W1,
                           u8 output[4 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    u8 d_mask[8];
    u8 m0_mask[8];
    const u8 D_byte = W0 ^ W1;
    for (int p = 0; p < 8; ++p) {
        const int shift = 7 - p;
        d_mask[p] = ((D_byte >> shift) & 1) ? 0xFFu : 0x00u;
        m0_mask[p] = ((W0 >> shift) & 1) ? 0xFFu : 0x00u;
    }
    const u32 aLo = spreadA(lanes[0]) | (spreadA(lanes[1]) << 1)
                  | (spreadA(lanes[2]) << 2) | (spreadA(lanes[3]) << 3);
    const u32 bLo = spreadB(lanes[0]) | (spreadB(lanes[1]) << 1)
                  | (spreadB(lanes[2]) << 2) | (spreadB(lanes[3]) << 3);
    u8 cols[8];
    cols[0] = static_cast<u8>(aLo);
    cols[1] = static_cast<u8>(aLo >> 8);
    cols[2] = static_cast<u8>(aLo >> 16);
    cols[3] = static_cast<u8>(aLo >> 24);
    cols[4] = static_cast<u8>(bLo);
    cols[5] = static_cast<u8>(bLo >> 8);
    cols[6] = static_cast<u8>(bLo >> 16);
    cols[7] = static_cast<u8>(bLo >> 24);
    for (int s = 0; s < 8; ++s) {
        const u8 col = cols[s];
        for (int k = 0; k < 4; ++k) {
            const int p_hi = 2 * k;
            const int p_lo = 2 * k + 1;
            const u8 hi = static_cast<u8>((m0_mask[p_hi] & 0xF0u) ^ ((col << 4) & d_mask[p_hi]));
            const u8 lo = static_cast<u8>((m0_mask[p_lo] & 0x0Fu) ^ (col & d_mask[p_lo]));
            output[s * 4 + k] = static_cast<u8>(hi | lo);
        }
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_2_bf1(const u8 lanes[2],
                           u8 W0, u8 W1,
                           u8 output[2 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    u8 d_mask[8];
    u8 m0_mask[8];
    const u8 D_byte = W0 ^ W1;
    for (int p = 0; p < 8; ++p) {
        const int shift = 7 - p;
        d_mask[p] = ((D_byte >> shift) & 1) ? 0xFFu : 0x00u;
        m0_mask[p] = ((W0 >> shift) & 1) ? 0xFFu : 0x00u;
    }
    for (int s = 0; s < 8; ++s) {
        const int bit_idx = 7 - s;
        const u8 b0 = static_cast<u8>((lanes[0] >> bit_idx) & 1u);
        const u8 b1 = static_cast<u8>((lanes[1] >> bit_idx) & 1u);
        u8 byte_hi = 0;
        u8 byte_lo = 0;
        for (int q = 0; q < 4; ++q) {
            const int p_hi = 3 - q;
            const int p_lo = 7 - q;
            const u8 m0_p_hi = static_cast<u8>(m0_mask[p_hi] & 1u);
            const u8 d_p_hi  = static_cast<u8>(d_mask[p_hi] & 1u);
            const u8 m0_p_lo = static_cast<u8>(m0_mask[p_lo] & 1u);
            const u8 d_p_lo  = static_cast<u8>(d_mask[p_lo] & 1u);
            const u8 v0_hi = static_cast<u8>(m0_p_hi ^ (b0 & d_p_hi));
            const u8 v1_hi = static_cast<u8>(m0_p_hi ^ (b1 & d_p_hi));
            const u8 v0_lo = static_cast<u8>(m0_p_lo ^ (b0 & d_p_lo));
            const u8 v1_lo = static_cast<u8>(m0_p_lo ^ (b1 & d_p_lo));
            byte_hi |= static_cast<u8>((v1_hi << (2 * q)) | (v0_hi << (2 * q + 1)));
            byte_lo |= static_cast<u8>((v1_lo << (2 * q)) | (v0_lo << (2 * q + 1)));
        }
        output[s * 2 + 0] = byte_hi;
        output[s * 2 + 1] = byte_lo;
    }
}

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16x4_bf1_pipe4(const u8 lanes_a[16],
                                    const u8 lanes_b[16],
                                    const u8 lanes_c[16],
                                    const u8 lanes_d[16],
                                    u8 W0, u8 W1,
                                    u8 output_a[16 * sizeof(Wave8Byte)],
                                    u8 output_b[16 * sizeof(Wave8Byte)],
                                    u8 output_c[16 * sizeof(Wave8Byte)],
                                    u8 output_d[16 * sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    u8 d_mask[8];
    u8 m0_mask[8];
    const u8 D_byte = W0 ^ W1;
    for (int p = 0; p < 8; ++p) {
        const int shift = 7 - p;
        d_mask[p] = ((D_byte >> shift) & 1) ? 0xFFu : 0x00u;
        m0_mask[p] = ((W0 >> shift) & 1) ? 0xFFu : 0x00u;
    }
    u8 cols_a[16], cols_b[16], cols_c[16], cols_d[16];
    spread_transpose16_symbol(lanes_a, cols_a);
    spread_transpose16_symbol(lanes_b, cols_b);
    spread_transpose16_symbol(lanes_c, cols_c);
    spread_transpose16_symbol(lanes_d, cols_d);
    for (int s = 0; s < 8; ++s) {
        const u8 al = cols_a[2*s + 0], ah = cols_a[2*s + 1];
        const u8 bl = cols_b[2*s + 0], bh = cols_b[2*s + 1];
        const u8 cl = cols_c[2*s + 0], ch = cols_c[2*s + 1];
        const u8 dl = cols_d[2*s + 0], dh = cols_d[2*s + 1];
        for (int p = 0; p < 8; ++p) {
            const u8 dm = d_mask[p], mm = m0_mask[p];
            output_a[s*16 + p*2 + 0] = mm ^ (al & dm);
            output_a[s*16 + p*2 + 1] = mm ^ (ah & dm);
            output_b[s*16 + p*2 + 0] = mm ^ (bl & dm);
            output_b[s*16 + p*2 + 1] = mm ^ (bh & dm);
            output_c[s*16 + p*2 + 0] = mm ^ (cl & dm);
            output_c[s*16 + p*2 + 1] = mm ^ (ch & dm);
            output_d[s*16 + p*2 + 0] = mm ^ (dl & dm);
            output_d[s*16 + p*2 + 1] = mm ^ (dh & dm);
        }
    }
}

} // namespace detail

FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8(u8 lane,
           const Wave8BitExpansionLut& lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]) FL_NO_EXCEPT {
    Wave8Byte waveformSymbol;
    detail::wave8_convert_byte_to_wave8byte(lane, lut, &waveformSymbol);
    isr::memcpy_32(fl::bit_cast_ptr<u32>(output),
                  fl::bit_cast_ptr<const u32>(&waveformSymbol.symbols[0].data),
                  2);
}

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
