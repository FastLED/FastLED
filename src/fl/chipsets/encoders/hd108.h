#pragma once

/// @file chipsets/encoders/hd108.h
/// @brief HD108 SPI chipset encoder
///
/// Free function encoder for HD108 chipsets.
/// HD108 uses 16-bit RGB with gamma correction and brightness control.
///
/// Protocol:
/// - Start frame: 64 bits (8 bytes) of 0x00
/// - LED data: [Header:2B][R:16b][G:16b][B:16b] (8 bytes per LED)
/// - End frame: (num_leds / 2) + 4 bytes of 0xFF
///
/// Header encoding (per-channel gain control):
/// - Byte 0: [1][RRRRR][GG] - marker bit, 5-bit R gain, 2 MSBs of G gain
/// - Byte 1: [GGG][BBBBB]   - 3 LSBs of G gain, 5-bit B gain
/// - Currently: R=G=B=brightness (same gain for all channels)
/// - Future: Per channel gain control for higher color range

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/chipsets/encoders/encoder_constants.h"

namespace fl {

/// @brief Encode pixel data in HD108 format with global brightness
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @param global_brightness Global 8-bit brightness (0-255), default 255
/// @note Uses gamma 2.8 correction for 16-bit RGB
/// @note HD108 uses RGB wire order: pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue
template <typename InputIterator, typename OutputIterator>
void encodeHD108(InputIterator first, InputIterator last, OutputIterator out,
                 u8 global_brightness = 255) {
    // Start frame: 64 bits (8 bytes) of 0x00
    for (int i = 0; i < 8; i++) {
        *out++ = 0x00;
    }

    // Compute brightness header bytes (cached for all LEDs)
    u8 f0, f1;
    hd108BrightnessHeader(global_brightness, &f0, &f1);

    // LED data: 2-byte header + 6-byte RGB16 (count as we go)
    size_t num_leds = 0;
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;

        // Apply gamma correction (2.8) to 16-bit (RGB order: 0=R, 1=G, 2=B)
        u16 r16 = hd108GammaCorrect(pixel[0]);  // Red
        u16 g16 = hd108GammaCorrect(pixel[1]);  // Green
        u16 b16 = hd108GammaCorrect(pixel[2]);  // Blue

        // Transmit: 2 header + 6 color bytes (16-bit RGB, big-endian)
        *out++ = f0;
        *out++ = f1;
        *out++ = static_cast<u8>(r16 >> 8);
        *out++ = static_cast<u8>(r16 & 0xFF);
        *out++ = static_cast<u8>(g16 >> 8);
        *out++ = static_cast<u8>(g16 & 0xFF);
        *out++ = static_cast<u8>(b16 >> 8);
        *out++ = static_cast<u8>(b16 & 0xFF);

        ++first;
        ++num_leds;
    }

    // End frame: (num_leds / 2) + 4 bytes of 0xFF
    const size_t latch = num_leds / 2 + 4;
    for (size_t i = 0; i < latch; i++) {
        *out++ = 0xFF;
    }
}

/// @brief Encode pixel data in HD108 format with per-LED brightness
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam BrightnessIterator Iterator yielding uint8_t brightness values
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param brightness_first Iterator to first brightness value (8-bit, 0-255)
/// @param out Output iterator for encoded bytes
/// @note HD108 uses RGB wire order: pixel[0]=Red, pixel[1]=Green, pixel[2]=Blue
template <typename InputIterator, typename BrightnessIterator, typename OutputIterator>
void encodeHD108_HD(InputIterator first, InputIterator last,
                    BrightnessIterator brightness_first, OutputIterator out) {
    // Start frame: 64 bits (8 bytes) of 0x00
    for (int i = 0; i < 8; i++) {
        *out++ = 0x00;
    }

    // Brightness conversion cache (avoid recomputing same values)
    u8 lastBrightness8 = 0;
    u8 lastF0 = 0xFF, lastF1 = 0xFF;  // Invalid markers

    // LED data: 2-byte header + 6-byte RGB16 (count as we go)
    size_t num_leds = 0;
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        u8 brightness = *brightness_first;

        // Apply gamma correction (2.8) to 16-bit (RGB order: 0=R, 1=G, 2=B)
        u16 r16 = hd108GammaCorrect(pixel[0]);  // Red
        u16 g16 = hd108GammaCorrect(pixel[1]);  // Green
        u16 b16 = hd108GammaCorrect(pixel[2]);  // Blue

        // Compute brightness header (with caching)
        u8 f0, f1;
        if (brightness == lastBrightness8 && lastF0 != 0xFF) {
            f0 = lastF0;
            f1 = lastF1;
        } else {
            hd108BrightnessHeader(brightness, &f0, &f1);
            lastBrightness8 = brightness;
            lastF0 = f0;
            lastF1 = f1;
        }

        // Transmit: 2 header + 6 color bytes (16-bit RGB, big-endian)
        *out++ = f0;
        *out++ = f1;
        *out++ = static_cast<u8>(r16 >> 8);
        *out++ = static_cast<u8>(r16 & 0xFF);
        *out++ = static_cast<u8>(g16 >> 8);
        *out++ = static_cast<u8>(g16 & 0xFF);
        *out++ = static_cast<u8>(b16 >> 8);
        *out++ = static_cast<u8>(b16 & 0xFF);

        ++first;
        ++brightness_first;
        ++num_leds;
    }

    // End frame: (num_leds / 2) + 4 bytes of 0xFF
    const size_t latch = num_leds / 2 + 4;
    for (size_t i = 0; i < latch; i++) {
        *out++ = 0xFF;
    }
}

} // namespace fl
