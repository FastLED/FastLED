#pragma once

/// @file fl/chipsets/encoders/ws2816.h
/// @brief WS2816 encoder - converts 16-bit RGB pixels to dual 8-bit RGB format
///
/// The WS2816 is a high-definition LED chipset that uses 16-bit color depth.
/// This encoder converts each 16-bit RGB pixel into two 8-bit RGB pixels
/// for transmission via standard WS2812-compatible controllers.
///
/// Protocol:
/// - Input: 16-bit RGB (3x 16-bit values = 48 bits per LED)
/// - Output: Dual 8-bit RGB (2x 24-bit CRGB = 48 bits per LED)
/// - Each 16-bit channel is split: high byte → first CRGB, low byte → second CRGB
/// - Channel layout: R_hi, R_lo, G_hi → first CRGB; G_lo, B_hi, B_lo → second CRGB
///
/// @note This encoder produces 2x CRGB output for each input pixel

#include "fl/stl/stdint.h"
#include "fl/stl/pair.h"
#include "crgb.h"

namespace fl {

/// @brief Pack a single 16-bit RGB pixel into two 8-bit CRGB pixels for WS2816
/// @param s0 First 16-bit channel value (R)
/// @param s1 Second 16-bit channel value (G)
/// @param s2 Third 16-bit channel value (B)
/// @return Pair of CRGB pixels encoding the 16-bit values
/// @note Channel layout: [R_hi, R_lo, G_hi] and [G_lo, B_hi, B_lo]
inline pair<CRGB, CRGB> packWS2816Pixel(u16 s0, u16 s1, u16 s2) {
    // Split each 16-bit channel into high/low bytes
    u8 b0_hi = s0 >> 8;
    u8 b0_lo = s0 & 0xFF;
    u8 b1_hi = s1 >> 8;
    u8 b1_lo = s1 & 0xFF;
    u8 b2_hi = s2 >> 8;
    u8 b2_lo = s2 & 0xFF;

    // Pack into two CRGB pixels: [R_hi, R_lo, G_hi] and [G_lo, B_hi, B_lo]
    return make_pair(
        CRGB(b0_hi, b0_lo, b1_hi),
        CRGB(b1_lo, b2_hi, b2_lo)
    );
}

/// @brief Encode 16-bit RGB pixel data into dual 8-bit RGB format for WS2816
/// @tparam InputIterator Iterator providing access to loadAndScale_WS2816_HD(u16*, u16*, u16*)
/// @tparam OutputIterator Output iterator accepting CRGB (e.g., raw pointer)
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel (sentinel)
/// @param out Output iterator for encoded CRGB pairs
/// @note Each input pixel yields 2 CRGB output pixels (48 bits → 2×24 bits)
template <typename InputIterator, typename OutputIterator>
void encodeWS2816(InputIterator first, InputIterator last, OutputIterator out) {
    while (first != last) {
        u16 s0, s1, s2;
        first->loadAndScale_WS2816_HD(&s0, &s1, &s2);

        // Use the packing helper function
        pair<CRGB, CRGB> packed = packWS2816Pixel(s0, s1, s2);
        *out++ = packed.first;
        *out++ = packed.second;

        ++first;
    }
}

} // namespace fl
