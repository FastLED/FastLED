/// @file wave3.cpp
/// @brief #2524: the optimized two-pass (8x8) wave3 lane transposes must be
/// bit-for-bit identical to a ground-truth naive transpose (8-lane and
/// 16-lane), across edge + random inputs.

#include "fl/channels/wave3.h"
#include "fl/channels/detail/wave3.hpp"
#include "fl/stl/cstring.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("wave3 transpose_8/_16 == naive (random + edge)") {
    // 8-lane: out[sym*8 + k] is pulse (7-k); bit `lane` = lane's bit (7-k).
    auto naive8 = [](const Wave3Byte in[8], u8 out[8 * sizeof(Wave3Byte)]) {
        for (int s = 0; s < 3; s++) {
            for (int k = 0; k < 8; k++) {
                int b = 7 - k;
                int byte = 0;
                for (int l = 0; l < 8; l++) {
                    byte |= ((in[l].data[s] >> b) & 1) << l;
                }
                out[s * 8 + k] = static_cast<u8>(byte);
            }
        }
    };
    // 16-lane: out[sym*16 + (7-b)*2] = lanes 0-7 (low), +1 = lanes 8-15 (high).
    auto naive16 = [](const Wave3Byte in[16], u8 out[16 * sizeof(Wave3Byte)]) {
        for (int s = 0; s < 3; s++) {
            for (int b = 0; b < 8; b++) {
                int lo = 0;
                int hi = 0;
                for (int l = 0; l < 8; l++) {
                    lo |= ((in[l].data[s] >> b) & 1) << l;
                    hi |= ((in[l + 8].data[s] >> b) & 1) << l;
                }
                out[s * 16 + (7 - b) * 2] = static_cast<u8>(lo);
                out[s * 16 + (7 - b) * 2 + 1] = static_cast<u8>(hi);
            }
        }
    };

    u32 seed = 0x13579BDFu;
    for (int round = 0; round < 400; round++) {
        Wave3Byte w3[16];
        for (int l = 0; l < 16; l++) {
            for (int s = 0; s < 3; s++) {
                u8 v;
                if (round == 0) {
                    v = 0x00;
                } else if (round == 1) {
                    v = 0xFF;
                } else if (round == 2) {
                    v = static_cast<u8>((l & 1) ? 0xAA : 0x55);
                } else {
                    seed = seed * 1664525u + 1013904223u;
                    v = static_cast<u8>(seed >> 24);
                }
                w3[l].data[s] = v;
            }
        }
        u8 got[16 * sizeof(Wave3Byte)];
        u8 exp[16 * sizeof(Wave3Byte)];

        fl::detail::wave3_transpose_8(w3, got);
        naive8(w3, exp);
        for (int i = 0; i < 8 * static_cast<int>(sizeof(Wave3Byte)); i++) {
            FL_REQUIRE(got[i] == exp[i]);
        }

        fl::detail::wave3_transpose_16(w3, got);
        naive16(w3, exp);
        for (int i = 0; i < 16 * static_cast<int>(sizeof(Wave3Byte)); i++) {
            FL_REQUIRE(got[i] == exp[i]);
        }
    }
}

} // FL_TEST_FILE
