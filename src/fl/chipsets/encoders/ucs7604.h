#pragma once

/// @file chipsets/encoders/ucs7604.h
/// @brief UCS7604 LED chipset encoder implementation
///
/// Free function encoder for UCS7604 chipsets.
/// Supports multiple modes: 8-bit/16-bit color depth, RGB/RGBW output.
///
/// Protocol:
/// - Preamble: 15 bytes (sync, header, mode, current control, reserved)
/// - Padding: 0-2 zero bytes (ensures total size divisible by 3)
/// - LED data: Variable size based on mode and LED count
///   - 8-bit RGB: 3 bytes/LED
///   - 8-bit RGBW: 4 bytes/LED
///   - 16-bit RGB: 6 bytes/LED
///   - 16-bit RGBW: 8 bytes/LED
///
/// @note This consolidates encoding logic previously in UCS7604ControllerT::showPixels

#include "fl/stl/stdint.h"
#include "fl/ease.h"

namespace fl {

/// @brief UCS7604 protocol configuration modes
enum UCS7604Mode {
    UCS7604_MODE_8BIT_800KHZ = 0x03,
    UCS7604_MODE_16BIT_800KHZ = 0x8B,
    UCS7604_MODE_16BIT_1600KHZ = 0x9B // not implemented because of timing difference.
};

/// @brief UCS7604 current control structure with 4-bit fields for each channel
struct UCS7604CurrentControl {
    uint8_t r;  ///< Red channel current (0x0-0xF)
    uint8_t g;  ///< Green channel current (0x0-0xF)
    uint8_t b;  ///< Blue channel current (0x0-0xF)
    uint8_t w;  ///< White channel current (0x0-0xF)

    /// Default constructor - maximum brightness
    UCS7604CurrentControl() : r(0xF), g(0xF), b(0xF), w(0xF) {}

    /// Construct from single brightness value (all channels)
    explicit UCS7604CurrentControl(uint8_t brightness)
        : r(brightness & 0xF), g(brightness & 0xF), b(brightness & 0xF), w(brightness & 0xF) {}

    /// Construct from individual channel values
    UCS7604CurrentControl(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t w_)
        : r(r_ & 0xF), g(g_ & 0xF), b(b_ & 0xF), w(w_ & 0xF) {}
};

/// @brief Build UCS7604 preamble (15 bytes)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param out Output iterator for preamble bytes
/// @param mode UCS7604 protocol mode (8-bit/16-bit)
/// @param r_current Red channel current control (0x0-0xF, wire order)
/// @param g_current Green channel current control (0x0-0xF, wire order)
/// @param b_current Blue channel current control (0x0-0xF, wire order)
/// @param w_current White channel current control (0x0-0xF, wire order)
/// @note Current control values should already be reordered to match wire protocol (RGB)
template <typename OutputIterator>
void buildUCS7604Preamble(OutputIterator out, UCS7604Mode mode,
                          uint8_t r_current, uint8_t g_current,
                          uint8_t b_current, uint8_t w_current) {
    // Sync pattern (6 bytes)
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;

    // Header (2 bytes)
    *out++ = 0x00;
    *out++ = 0x02;

    // Mode byte
    *out++ = static_cast<uint8_t>(mode);

    // Current control (4 bytes, 4-bit each, wire order RGBW)
    *out++ = r_current & 0x0F;
    *out++ = g_current & 0x0F;
    *out++ = b_current & 0x0F;
    *out++ = w_current & 0x0F;

    // Reserved (2 bytes)
    *out++ = 0x00;
    *out++ = 0x00;
}

/// @brief Encode RGB pixels in UCS7604 8-bit format
/// @tparam PixelIteratorT PixelIterator or compatible type
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param pixel_iter PixelIterator with has() and loadAndScaleRGB() methods
/// @param num_leds Number of LEDs to encode
/// @param out Output iterator for encoded bytes
/// @note Writes 3 bytes per pixel (RGB order)
template <typename PixelIteratorT, typename OutputIterator>
void encodeUCS7604_8bit_RGB(PixelIteratorT& pixel_iter, size_t num_leds, OutputIterator out) {
    for (size_t i = 0; i < num_leds && pixel_iter.has(1); ++i) {
        uint8_t r, g, b;
        pixel_iter.loadAndScaleRGB(&r, &g, &b);
        pixel_iter.advanceData();
        *out++ = r;
        *out++ = g;
        *out++ = b;
    }
}

/// @brief Encode RGBW pixels in UCS7604 8-bit format
/// @tparam PixelIteratorT PixelIterator or compatible type
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param pixel_iter PixelIterator with has() and loadAndScaleRGBW() methods
/// @param num_leds Number of LEDs to encode
/// @param out Output iterator for encoded bytes
/// @note Writes 4 bytes per pixel (RGBW order)
template <typename PixelIteratorT, typename OutputIterator>
void encodeUCS7604_8bit_RGBW(PixelIteratorT& pixel_iter, size_t num_leds, OutputIterator out) {
    for (size_t i = 0; i < num_leds && pixel_iter.has(1); ++i) {
        uint8_t r, g, b, w;
        pixel_iter.loadAndScaleRGBW(&r, &g, &b, &w);
        pixel_iter.advanceData();
        *out++ = r;
        *out++ = g;
        *out++ = b;
        *out++ = w;
    }
}

/// @brief Encode RGB pixels in UCS7604 16-bit format with gamma correction
/// @tparam PixelIteratorT PixelIterator or compatible type
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param pixel_iter PixelIterator with has() and loadAndScaleRGB() methods
/// @param num_leds Number of LEDs to encode
/// @param out Output iterator for encoded bytes
/// @note Writes 6 bytes per pixel (R16_hi, R16_lo, G16_hi, G16_lo, B16_hi, B16_lo)
/// @note Applies gamma 2.8 correction to expand 8-bit to 16-bit values
template <typename PixelIteratorT, typename OutputIterator>
void encodeUCS7604_16bit_RGB(PixelIteratorT& pixel_iter, size_t num_leds, OutputIterator out) {
    for (size_t i = 0; i < num_leds && pixel_iter.has(1); ++i) {
        uint8_t r, g, b;
        pixel_iter.loadAndScaleRGB(&r, &g, &b);
        pixel_iter.advanceData();

        // Apply gamma 2.8 correction for 16-bit output
        uint16_t r16 = fl::gamma_2_8(r);
        uint16_t g16 = fl::gamma_2_8(g);
        uint16_t b16 = fl::gamma_2_8(b);

        // Write big-endian 16-bit values
        *out++ = r16 >> 8;
        *out++ = r16 & 0xFF;
        *out++ = g16 >> 8;
        *out++ = g16 & 0xFF;
        *out++ = b16 >> 8;
        *out++ = b16 & 0xFF;
    }
}

/// @brief Encode RGBW pixels in UCS7604 16-bit format with gamma correction
/// @tparam PixelIteratorT PixelIterator or compatible type
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param pixel_iter PixelIterator with has() and loadAndScaleRGBW() methods
/// @param num_leds Number of LEDs to encode
/// @param out Output iterator for encoded bytes
/// @note Writes 8 bytes per pixel (R16_hi, R16_lo, G16_hi, G16_lo, B16_hi, B16_lo, W16_hi, W16_lo)
/// @note Applies gamma 2.8 correction to expand 8-bit to 16-bit values
template <typename PixelIteratorT, typename OutputIterator>
void encodeUCS7604_16bit_RGBW(PixelIteratorT& pixel_iter, size_t num_leds, OutputIterator out) {
    for (size_t i = 0; i < num_leds && pixel_iter.has(1); ++i) {
        uint8_t r, g, b, w;
        pixel_iter.loadAndScaleRGBW(&r, &g, &b, &w);
        pixel_iter.advanceData();

        // Apply gamma 2.8 correction for 16-bit output
        uint16_t r16 = fl::gamma_2_8(r);
        uint16_t g16 = fl::gamma_2_8(g);
        uint16_t b16 = fl::gamma_2_8(b);
        uint16_t w16 = fl::gamma_2_8(w);

        // Write big-endian 16-bit values
        *out++ = r16 >> 8;
        *out++ = r16 & 0xFF;
        *out++ = g16 >> 8;
        *out++ = g16 & 0xFF;
        *out++ = b16 >> 8;
        *out++ = b16 & 0xFF;
        *out++ = w16 >> 8;
        *out++ = w16 & 0xFF;
    }
}

/// @brief Encode complete UCS7604 frame (preamble + padding + pixel data)
/// @tparam PixelIteratorT PixelIterator or compatible type
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param pixel_iter PixelIterator with has(), advanceData(), loadAndScaleRGB/RGBW() methods
/// @param num_leds Number of LEDs to encode
/// @param out Output iterator for complete frame
/// @param mode UCS7604 protocol mode (8-bit/16-bit)
/// @param current Current control settings (wire order RGBW)
/// @param is_rgbw True for RGBW mode, false for RGB mode
/// @note Outputs: preamble (15 bytes) + padding (0-2 bytes) + LED data
/// @note Total output size is always divisible by 3 (required by UCS7604 protocol)
template <typename PixelIteratorT, typename OutputIterator>
void encodeUCS7604(PixelIteratorT& pixel_iter, size_t num_leds, OutputIterator out,
                   UCS7604Mode mode, const UCS7604CurrentControl& current, bool is_rgbw) {
    constexpr size_t PREAMBLE_LEN = 15;

    // Calculate bytes per LED based on mode and RGB/RGBW
    size_t bytes_per_led;
    if (mode == UCS7604_MODE_8BIT_800KHZ) {
        bytes_per_led = is_rgbw ? 4 : 3;
    } else {
        bytes_per_led = is_rgbw ? 8 : 6;
    }

    // Calculate total data size and padding
    size_t led_data_size = num_leds * bytes_per_led;
    size_t total_data_size = PREAMBLE_LEN + led_data_size;
    size_t padding = (3 - (total_data_size % 3)) % 3;

    // Build preamble (15 bytes) with current control
    buildUCS7604Preamble(out, mode, current.r, current.g, current.b, current.w);

    // Add padding (0-2 zero bytes)
    for (size_t i = 0; i < padding; ++i) {
        *out++ = 0;
    }

    // Encode LED data based on mode and RGB/RGBW
    if (mode == UCS7604_MODE_8BIT_800KHZ) {
        if (is_rgbw) {
            encodeUCS7604_8bit_RGBW(pixel_iter, num_leds, out);
        } else {
            encodeUCS7604_8bit_RGB(pixel_iter, num_leds, out);
        }
    } else {
        // 16-bit modes
        if (is_rgbw) {
            encodeUCS7604_16bit_RGBW(pixel_iter, num_leds, out);
        } else {
            encodeUCS7604_16bit_RGB(pixel_iter, num_leds, out);
        }
    }
}

} // namespace fl
