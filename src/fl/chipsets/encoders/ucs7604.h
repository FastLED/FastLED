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
#include "fl/math/ease.h"
#include "fl/chipsets/encoders/pixel_iterator_adapters.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep  (FL_UNUSED)
#include "fl/stl/noexcept.h"
#include "fl/system/sketch_macros.h"  // IWYU pragma: keep  (FL_PLATFORM_HAS_TINY_MEMORY)

namespace fl {

/// @brief UCS7604 protocol configuration modes
enum class UCS7604Mode {
    UCS7604_MODE_8BIT_800KHZ = 0x03,
    UCS7604_MODE_16BIT_800KHZ = 0x8B,
    UCS7604_MODE_16BIT_1600KHZ = 0x9B // not implemented because of timing difference.
};

/// @brief UCS7604 current control structure with 4-bit fields for each channel
struct UCS7604CurrentControl {
    u8 r;  ///< Red channel current (0x0-0xF)
    u8 g;  ///< Green channel current (0x0-0xF)
    u8 b;  ///< Blue channel current (0x0-0xF)
    u8 w;  ///< White channel current (0x0-0xF)

    /// Default constructor - maximum brightness
    UCS7604CurrentControl() FL_NO_EXCEPT : r(0xF), g(0xF), b(0xF), w(0xF) {}

    /// Construct from single brightness value (all channels)
    explicit UCS7604CurrentControl(u8 brightness)
        : r(brightness & 0xF), g(brightness & 0xF), b(brightness & 0xF), w(brightness & 0xF) {}

    /// Construct from individual channel values
    UCS7604CurrentControl(u8 r_, u8 g_, u8 b_, u8 w_)
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
/// @note KNOWN LIMITATION: The UCS7604 protocol spec requires a ~20us "W-code low"
/// delay between the 8-byte verification code and the 7-byte configuration block.
/// Our clockless controller sends all 15 bytes as a continuous bit-encoded stream
/// without this gap. If this causes issues on some hardware, the transmission would
/// need to be split into two separate clockless sends with a manual low-hold between
/// them. See: https://github.com/clinder/Arduino-Teensy-UCS7604
template <typename OutputIterator>
void buildUCS7604Preamble(OutputIterator out, UCS7604Mode mode,
                          u8 r_current, u8 g_current,
                          u8 b_current, u8 w_current) {
    // Sync pattern (6 bytes)
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;
    *out++ = 0xFF;

    // Header (2 bytes)
    *out++ = 0x00;
    *out++ = 0x03;

    // Mode byte
    *out++ = static_cast<u8>(mode);

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
/// @tparam InputIterator Iterator yielding fl::array<uint8_t, 3> (RGB bytes)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Beginning of pixel range
/// @param last End of pixel range
/// @param out Output iterator for encoded bytes
/// @note Writes 3 bytes per pixel (RGB order)
template <typename InputIterator, typename OutputIterator>
void encodeUCS7604_8bit_RGB(InputIterator first, InputIterator last, OutputIterator out) {
    while (first != last) {
        const auto& pixel = *first;
        *out++ = pixel[0];  // R
        *out++ = pixel[1];  // G
        *out++ = pixel[2];  // B
        ++first;
    }
}

/// @brief Encode RGBW pixels in UCS7604 8-bit format
/// @tparam InputIterator Iterator yielding fl::array<uint8_t, 4> (RGBW bytes)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Beginning of pixel range
/// @param last End of pixel range
/// @param out Output iterator for encoded bytes
/// @note Writes 4 bytes per pixel (RGBW order)
template <typename InputIterator, typename OutputIterator>
void encodeUCS7604_8bit_RGBW(InputIterator first, InputIterator last, OutputIterator out) {
    while (first != last) {
        const auto& pixel = *first;
        *out++ = pixel[0];  // R
        *out++ = pixel[1];  // G
        *out++ = pixel[2];  // B
        *out++ = pixel[3];  // W
        ++first;
    }
}

/// @brief Encode RGB pixels in UCS7604 16-bit format with gamma correction
/// @tparam InputIterator Iterator yielding fl::array<uint8_t, 3> (RGB bytes)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Beginning of pixel range
/// @param last End of pixel range
/// @param out Output iterator for encoded bytes
/// @param gamma Gamma8 LUT for 8-to-16 bit expansion
/// @note Writes 6 bytes per pixel (R16_hi, R16_lo, G16_hi, G16_lo, B16_hi, B16_lo)
template <typename InputIterator, typename OutputIterator>
void encodeUCS7604_16bit_RGB(InputIterator first, InputIterator last, OutputIterator out,
                              const Gamma8& gamma) FL_NO_EXCEPT {
    while (first != last) {
        const auto& pixel = *first;

        // Apply gamma correction for 16-bit output
        u8 rgb_in[3] = { pixel[0], pixel[1], pixel[2] };
        u16 rgb_out[3];
        gamma.convert(fl::span<const u8>(rgb_in, 3), fl::span<u16>(rgb_out, 3));

        // Write big-endian 16-bit values
        *out++ = rgb_out[0] >> 8;
        *out++ = rgb_out[0] & 0xFF;
        *out++ = rgb_out[1] >> 8;
        *out++ = rgb_out[1] & 0xFF;
        *out++ = rgb_out[2] >> 8;
        *out++ = rgb_out[2] & 0xFF;
        ++first;
    }
}

/// Wide RGB straight off the iterator, with no adapter (P8, #4042 / #4326).
///
/// TINY only: `PixelIterator::loadAndScaleRGB16` is itself compiled out on
/// parts with <=1KB SRAM, which carry no colour pipeline and so have nothing
/// wider than the 8-bit pixel to load. `pixels` is a concrete
/// `PixelIterator&`, not a dependent type, so that call is looked up when
/// this template is *defined* rather than when it is instantiated -- leaving
/// the body visible on TINY is a hard compile error even though no caller
/// ever selects it. Hence the guard here, and the matching one around the
/// `wide_source` branch in `encodeUCS7604`.
///
/// Deliberately a plain loop over `PixelIterator` rather than a second
/// `makeScaledPixelRange*` range. `fl::Channel::showPixels` keeps every
/// `writeUCS7604(...)` statically reachable, so whatever this path
/// instantiates is linked into sketches that never touch UCS7604 -- routing
/// it through `ScaledPixelIteratorRGB16` cost 988 B on an ESP32-S3 Blink
/// build that binds no profile. This does what that adapter does, in the same
/// order, without the iterator-pair templates.
///
/// No gamma by construction: the source has already quantized its device
/// drive once, to 16 bits, and a curve on top of that is the second shaping
/// stage B1 and section 6 of the spec forbid after the device solve.
#if !FL_PLATFORM_HAS_TINY_MEMORY
template <typename OutputIterator>
void encodeUCS7604_16bit_RGB_wide(PixelIterator& pixels, OutputIterator out) FL_NO_EXCEPT {
    while (pixels.has(1)) {
        u16 r16, g16, b16;
        pixels.loadAndScaleRGB16(&r16, &g16, &b16);
        *out++ = r16 >> 8;
        *out++ = r16 & 0xFF;
        *out++ = g16 >> 8;
        *out++ = g16 & 0xFF;
        *out++ = b16 >> 8;
        *out++ = b16 & 0xFF;
        pixels.stepDithering();
        pixels.advanceData();
    }
}
#endif  // !FL_PLATFORM_HAS_TINY_MEMORY

/// @brief Encode RGBW pixels in UCS7604 16-bit format with gamma correction
/// @tparam InputIterator Iterator yielding fl::array<uint8_t, 4> (RGBW bytes)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param first Beginning of pixel range
/// @param last End of pixel range
/// @param out Output iterator for encoded bytes
/// @param gamma Gamma8 LUT for 8-to-16 bit expansion
/// @note Writes 8 bytes per pixel (R16_hi, R16_lo, G16_hi, G16_lo, B16_hi, B16_lo, W16_hi, W16_lo)
template <typename InputIterator, typename OutputIterator>
void encodeUCS7604_16bit_RGBW(InputIterator first, InputIterator last, OutputIterator out,
                               const Gamma8& gamma) {
    while (first != last) {
        const auto& pixel = *first;

        // Apply gamma correction for 16-bit output
        u8 rgbw_in[4] = { pixel[0], pixel[1], pixel[2], pixel[3] };
        u16 rgbw_out[4];
        gamma.convert(fl::span<const u8>(rgbw_in, 4), fl::span<u16>(rgbw_out, 4));

        // Write big-endian 16-bit values
        *out++ = rgbw_out[0] >> 8;
        *out++ = rgbw_out[0] & 0xFF;
        *out++ = rgbw_out[1] >> 8;
        *out++ = rgbw_out[1] & 0xFF;
        *out++ = rgbw_out[2] >> 8;
        *out++ = rgbw_out[2] & 0xFF;
        *out++ = rgbw_out[3] >> 8;
        *out++ = rgbw_out[3] & 0xFF;
        ++first;
    }
}

/// @brief Wire bytes each LED contributes to a UCS7604 frame
inline size_t ucs7604BytesPerLed(UCS7604Mode mode, bool is_rgbw) FL_NO_EXCEPT {
    if (mode == UCS7604Mode::UCS7604_MODE_8BIT_800KHZ) {
        return is_rgbw ? 4u : 3u;
    }
    return is_rgbw ? 8u : 6u;
}

/// @brief Total wire bytes of a complete UCS7604 frame
///
/// Preamble + padding + pixel data, the length `encodeUCS7604` writes. Callers
/// that must size a capture or predict whether a frame fits need this without
/// encoding it first (FastLED#4371); `encodeUCS7604` computes its own padding
/// from here so the two cannot drift.
inline size_t ucs7604FrameBytes(size_t num_leds, UCS7604Mode mode,
                                bool is_rgbw) FL_NO_EXCEPT {
    constexpr size_t kPreambleLen = 15;
    const size_t total = kPreambleLen + num_leds * ucs7604BytesPerLed(mode, is_rgbw);
    // The protocol requires the total to be divisible by 3.
    return total + (3u - (total % 3u)) % 3u;
}

/// @brief Encode complete UCS7604 frame (preamble + padding + pixel data)
/// @tparam OutputIterator Output iterator accepting uint8_t
/// @param pixel_iter PixelIterator with pixel data and scaling/gamma/dithering
/// @param num_leds Number of LEDs to encode
/// @param out Output iterator for complete frame
/// @param mode UCS7604 protocol mode (8-bit/16-bit)
/// @param current Current control settings (wire order RGBW)
/// @param is_rgbw True for RGBW mode, false for RGB mode
/// @param gamma Gamma8 LUT for 16-bit modes (nullable, ignored for 8-bit mode)
/// @note Outputs: preamble (15 bytes) + padding (0-2 bytes) + LED data
/// @note Total output size is always divisible by 3 (required by UCS7604 protocol)
template <typename OutputIterator>
void encodeUCS7604(PixelIterator& pixel_iter, size_t num_leds, OutputIterator out,
                   UCS7604Mode mode, const UCS7604CurrentControl& current, bool is_rgbw,
                   const Gamma8* gamma = nullptr,
                   bool wide_source = false) FL_NO_EXCEPT {
    constexpr size_t PREAMBLE_LEN = 15;

#if FL_PLATFORM_HAS_TINY_MEMORY
    // No wide encoder exists on TINY, so the flag selects nothing there.
    FL_UNUSED(wide_source);
#endif

    // Padding comes from ucs7604FrameBytes() so the length callers predict and
    // the length written here are the same number by construction.
    const size_t led_data_size = num_leds * ucs7604BytesPerLed(mode, is_rgbw);
    const size_t padding =
        ucs7604FrameBytes(num_leds, mode, is_rgbw) - PREAMBLE_LEN - led_data_size;

    // Build preamble (15 bytes) with current control
    buildUCS7604Preamble(out, mode, current.r, current.g, current.b, current.w);

    // Add padding (0-2 zero bytes)
    for (size_t i = 0; i < padding; ++i) {
        *out++ = 0;
    }

    // Encode LED data based on mode and RGB/RGBW
    if (mode == UCS7604Mode::UCS7604_MODE_8BIT_800KHZ) {
        if (is_rgbw) {
            auto range = makeScaledPixelRangeRGBW(&pixel_iter);
            encodeUCS7604_8bit_RGBW(range.first, range.second, out);
        } else {
            auto range = makeScaledPixelRangeRGB(&pixel_iter);
            encodeUCS7604_8bit_RGB(range.first, range.second, out);
        }
    } else {
        // 16-bit modes -- fall back to gamma 2.8 if no gamma provided
        static fl::shared_ptr<const Gamma8> default_gamma;
        const Gamma8& g = gamma ? *gamma : *(default_gamma ? default_gamma : (default_gamma = Gamma8::getOrCreate(2.8f)));
        if (is_rgbw) {
            // RGBW stays on the gamma path: `ColorManagedPixelSource`
            // delegates its RGBW entry point to the legacy controller, so
            // there is no wide drive to consume here yet.
            auto range = makeScaledPixelRangeRGBW(&pixel_iter);
            encodeUCS7604_16bit_RGBW(range.first, range.second, out, g);
#if !FL_PLATFORM_HAS_TINY_MEMORY
        } else if (wide_source) {
            encodeUCS7604_16bit_RGB_wide(pixel_iter, out);
#endif
        } else {
            auto range = makeScaledPixelRangeRGB(&pixel_iter);
            encodeUCS7604_16bit_RGB(range.first, range.second, out, g);
        }
    }
}

} // namespace fl
