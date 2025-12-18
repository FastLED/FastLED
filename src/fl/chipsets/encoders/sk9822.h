#pragma once

/// @file chipsets/encoders/sk9822.h
/// @brief SK9822 SPI chipset encoder
///
/// Free function encoder for SK9822 chipsets.
/// SK9822 is nearly identical to APA102, with one key difference:
/// end frame uses 0x00 instead of 0xFF.
///
/// Protocol:
/// - Start frame: 4 bytes of 0x00
/// - LED data: [0xE0|brightness][B][G][R] (4 bytes per LED)
/// - End frame: ⌈num_leds/32⌉ DWords of 0x00 (differs from APA102)
///
/// Brightness modes:
/// - Global: All LEDs use same 5-bit brightness
/// - Per-LED: Each LED has individual brightness
/// - Full: All LEDs at maximum brightness (31)

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/chipsets/encoders/encoder_utils.h"
#include "fl/chipsets/encoders/encoder_constants.h"
#include "fl/compiler_control.h"

namespace fl {

/// @brief Encode pixel data in SK9822 format with global brightness
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @param global_brightness Global 5-bit brightness (0-31), default 31
/// @note SK9822 uses BGR wire order: pixel[0]=Blue, pixel[1]=Green, pixel[2]=Red
template <typename InputIterator, typename OutputIterator>
void encodeSK9822(InputIterator first, InputIterator last, OutputIterator out,
                  u8 global_brightness = 31) {
    // Clamp brightness to 5-bit range
    global_brightness = global_brightness & 0x1F;

    // Start frame: 4 bytes of 0x00
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;

    // LED data: brightness header + BGR (count as we go)
    size_t num_leds = 0;
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        *out++ = 0xE0 | global_brightness;
        *out++ = pixel[0];  // Index 0 (BGR order: Blue)
        *out++ = pixel[1];  // Index 1 (BGR order: Green)
        *out++ = pixel[2];  // Index 2 (BGR order: Red)
        ++first;
        ++num_leds;
    }

    // SK9822 difference: end frame uses 0x00 instead of 0xFF
    size_t end_dwords = (num_leds / 32) + 1;
    for (size_t i = 0; i < end_dwords * 4; i++) {
        *out++ = 0x00;
    }
}

/// @brief Encode pixel data in SK9822 format with per-LED brightness
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam BrightnessIterator Iterator yielding uint8_t brightness values
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param brightness_first Iterator to first brightness value (8-bit, 0-255)
/// @param out Output iterator for encoded bytes
/// @note SK9822 uses BGR wire order: pixel[0]=Blue, pixel[1]=Green, pixel[2]=Red
template <typename InputIterator, typename BrightnessIterator, typename OutputIterator>
void encodeSK9822_HD(InputIterator first, InputIterator last,
                     BrightnessIterator brightness_first, OutputIterator out) {
    // Start frame: 4 bytes of 0x00
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;

    // LED data: brightness header + BGR (per-LED brightness, count as we go)
    size_t num_leds = 0;
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        u8 brightness_8bit = *brightness_first;
        u8 brightness_5bit = mapBrightness8to5(brightness_8bit);

        *out++ = 0xE0 | brightness_5bit;
        *out++ = pixel[0];  // Index 0 (BGR order: Blue)
        *out++ = pixel[1];  // Index 1 (BGR order: Green)
        *out++ = pixel[2];  // Index 2 (BGR order: Red)

        ++first;
        ++brightness_first;
        ++num_leds;
    }

    // SK9822 difference: end frame uses 0x00
    size_t end_dwords = (num_leds / 32) + 1;
    for (size_t i = 0; i < end_dwords * 4; i++) {
        *out++ = 0x00;
    }
}

/// @brief Encode pixel data in SK9822 format (auto-detected brightness from first pixel)
/// @tparam InputIterator Iterator yielding fl::array<u8, 3> (3 bytes in wire order)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Iterator to first pixel
/// @param last Iterator past last pixel
/// @param out Output iterator for encoded bytes
/// @note SK9822 uses BGR wire order: pixel[0]=Blue, pixel[1]=Green, pixel[2]=Red
/// @note Uses O2 optimization to avoid AVR register allocation issues with LTO
/// @note Marked noinline on AVR to prevent register exhaustion from divisions
template <typename InputIterator, typename OutputIterator>
FL_NO_INLINE_IF_AVR
void encodeSK9822_AutoBrightness(InputIterator first, InputIterator last,
                                 OutputIterator out) {
    if (first == last) {
        // Empty range - just write start frame
        *out++ = 0x00; *out++ = 0x00; *out++ = 0x00; *out++ = 0x00;
        return;
    }

    // Start frame
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;
    *out++ = 0x00;

    // Extract global brightness from first pixel
    const fl::array<u8, BYTES_PER_PIXEL_RGB>& first_pixel = *first;
    const u16 maxBrightness = 0x1F;
    // Use sequential comparisons to avoid nested FL_MAX (helps AVR register allocation)
    u8 max_rg = (first_pixel[2] > first_pixel[1]) ? first_pixel[2] : first_pixel[1];
    u8 max_component = (max_rg > first_pixel[0]) ? max_rg : first_pixel[0];
    u16 brightness = ((((u16)max_component + 1) * maxBrightness - 1) >> 8) + 1;
    u8 global_brightness = static_cast<u8>(brightness);

    // Write first LED with extracted brightness (BGR order: 0=B, 1=G, 2=R)
    u8 s0 = (maxBrightness * first_pixel[2] + (brightness >> 1)) / brightness;  // Red
    u8 s1 = (maxBrightness * first_pixel[1] + (brightness >> 1)) / brightness;  // Green
    u8 s2 = (maxBrightness * first_pixel[0] + (brightness >> 1)) / brightness;  // Blue

    *out++ = 0xE0 | (global_brightness & 0x1F);
    *out++ = s2;  // Blue
    *out++ = s1;  // Green
    *out++ = s0;  // Red
    ++first;

    // Write remaining LEDs (count as we go)
    size_t num_leds = 1;  // Already wrote first pixel
    while (first != last) {
        const fl::array<u8, BYTES_PER_PIXEL_RGB>& pixel = *first;
        *out++ = 0xE0 | (global_brightness & 0x1F);
        *out++ = pixel[0];  // Index 0 (BGR order: Blue)
        *out++ = pixel[1];  // Index 1 (BGR order: Green)
        *out++ = pixel[2];  // Index 2 (BGR order: Red)
        ++first;
        ++num_leds;
    }

    // End frame (SK9822 uses 0x00)
    size_t end_dwords = (num_leds / 32) + 1;
    for (size_t i = 0; i < end_dwords * 4; i++) {
        *out++ = 0x00;
    }
}

} // namespace fl
