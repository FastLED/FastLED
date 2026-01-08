#pragma once

/// @file chipsets/encoders/encoder_utils.h
/// @brief Shared utilities for SPI chipset encoders
///
/// Common functions used by multiple encoder implementations including
/// brightness mapping, checksum generation, and color scaling.

#include "fl/stl/stdint.h"
#include "fl/gamma.h"

namespace fl {

/// @brief Map 8-bit brightness to 5-bit (0-31)
/// @param brightness_8bit Input brightness (0-255)
/// @return 5-bit brightness (0-31)
/// @note Ensures non-zero input maps to non-zero output (fixes issue #1908)
/// @note Uses bit-shift approximation on AVR to avoid expensive division
inline u8 mapBrightness8to5(u8 brightness_8bit) {
    #if defined(__AVR__)
    // AVR-specific: Use bit shifts to avoid expensive division
    // Approximation: (value * 31) / 255 ≈ (value * 31) >> 8
    // Add rounding: ((value * 31) + 128) >> 8
    u16 bri5 = ((u16)brightness_8bit * 31 + 128) >> 8;

    // Ensure non-zero input doesn't map to zero output
    if (bri5 == 0 && brightness_8bit != 0) {
        bri5 = 1;
    }

    return static_cast<u8>(bri5);
    #else
    // Non-AVR: Use accurate division
    u16 bri5 = ((u16)brightness_8bit * 31 + 128) / 255;

    // Ensure non-zero input doesn't map to zero output
    if (bri5 == 0 && brightness_8bit != 0) {
        bri5 = 1;
    }

    return static_cast<u8>(bri5);
    #endif
}

/// @brief Generate P9813 flag byte from RGB components
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return P9813 flag byte (0xC0 | checksum)
/// @note Checksum uses top 2 bits of each color channel
inline u8 p9813FlagByte(u8 r, u8 g, u8 b) {
    return 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
}

/// @brief Apply LPD8806 encoding to a single color byte
/// @param value 8-bit color value (0-255)
/// @return 7-bit color with MSB set (0x80-0xFF)
/// @note LPD8806 uses 7-bit color depth with MSB always set
inline u8 lpd8806Encode(u8 value) {
    // LPD8806 requires MSB set on each byte, and uses 7-bit color depth
    return 0x80 | ((value >> 1) | ((value && (value < 254)) & 0x01));
}

/// @brief Convert 8-bit color to HD108 16-bit gamma-corrected value
/// @param value 8-bit color value (0-255)
/// @return 16-bit gamma-corrected value
/// @note Uses gamma 2.8 correction
inline u16 hd108GammaCorrect(u8 value) {
    return gamma_2_8(value);
}

/// @brief Generate HD108 per-channel gain header bytes
/// @param brightness_8bit 8-bit brightness (0-255) - UNUSED, kept for API compatibility
/// @param f0_out Output: first header byte
/// @param f1_out Output: second header byte
/// @note HD108 uses per-channel gain encoding: 5 bits each for R/G/B
/// @note All gains set to maximum (31) for maximum precision
/// @note Brightness control via 16-bit PWM values (applied before encoding)
/// @note Future: Per channel gain control for higher color range
inline void hd108BrightnessHeader(u8 brightness_8bit, u8* f0_out, u8* f1_out) {
    (void)brightness_8bit;  // Unused - brightness applied to 16-bit values instead

    // Use maximum gain for all channels for maximum precision
    // Brightness control happens via 16-bit PWM values
    constexpr u8 r_gain = 31;
    constexpr u8 g_gain = 31;
    constexpr u8 b_gain = 31;

    // Correct HD108 per-channel encoding:
    // f0: [1][RRRRR][GG] - marker bit, 5-bit R gain, 2 MSBs of G gain
    // f1: [GGG][BBBBB]   - 3 LSBs of G gain, 5-bit B gain
    *f0_out = 0x80 | ((r_gain & 0x1F) << 2) | ((g_gain >> 3) & 0x03);
    *f1_out = ((g_gain & 0x07) << 5) | (b_gain & 0x1F);
}

/// @brief Convert RGB to LPD6803 16-bit format (1bbbbbgggggrrrrr)
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return 16-bit LPD6803 command word
/// @note Bit 15: marker (1), Bits 14-0: 5-5-5 RGB
inline u16 lpd6803EncodeRGB(u8 r, u8 g, u8 b) {
    u16 command = 0x8000;  // Start marker
    command |= (r & 0xF8) << 7;   // Red: high 5 bits
    command |= (g & 0xF8) << 2;   // Green: high 5 bits
    command |= (b >> 3);          // Blue: high 5 bits
    return command;
}

} // namespace fl
