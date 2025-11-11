#pragma once

#ifndef __INC_UCS7604_H
#define __INC_UCS7604_H

#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "pixel_controller.h"
#include "cpixel_ledcontroller.h"
#include "fl/force_inline.h"
#include "fl/chipsets/led_timing.h"
#include "fl/pixel_iterator.h"
#include "fl/vector.h"
#include "lib8tion/intmap.h"
#include "fl/type_traits.h"
#include "fl/ease.h"

/// @file ucs7604.h
/// @brief UCS7604 LED chipset controller implementation for FastLED
///
/// The UCS7604 uses a unique preamble-based protocol. This implementation
/// extends ClocklessBlockingGeneric for universal platform support.
///
/// # Key Implementation Detail: Preamble Encoding
///
/// The 15-byte preamble divides perfectly into 5 CRGB pixels: **15 ÷ 3 = 5**
/// - Chunk 1 (8 bytes) + Chunk 2 (7 bytes) = 15 bytes total
/// - 15 bytes ÷ 3 bytes/pixel = 5 pixels exactly (no padding needed!)
/// - Transmitted as 5 fake CRGB values reinterpreted from preamble bytes
/// - GitHub #2088 confirmed continuous transmission works (no 260µs delays needed)
///
/// # Current Control
///
/// The UCS7604 has 4-bit current control (0x00-0x0F) for each RGBW channel.
/// Define FL_UCS7604_BRIGHTNESS to set the default current control value:
///   - 0x0F = Maximum brightness/current (default)
///   - 0x00 = Minimum brightness/current
/// This affects all channels (RGBW) unless overridden via setCurrentControl()
///
/// ## Runtime Brightness Control (EXPERIMENTAL)
///
/// Use fl::ucs7604::set_brightness(uint8_t) to adjust current control at runtime.
/// This is a hardware-level brightness control (4-bit, 16 levels) and is
/// SECONDARY to FastLED::setBrightness() which should be your primary brightness control.
///
/// Note: This uses the current control feature which may affect color accuracy.
/// For best results, use FastLED::setBrightness() for brightness control.

#ifndef FL_UCS7604_BRIGHTNESS
#define FL_UCS7604_BRIGHTNESS 0x0F
#endif

namespace fl {

/// @brief UCS7604 runtime brightness control namespace
namespace ucs7604 {
    /// @brief UCS7604 current control structure with 4-bit fields for each channel
    /// This is a union that allows both individual channel access and bulk assignment
    union CurrentControl {
        struct {
            uint8_t r : 4;  ///< Red channel current (0x0-0xF)
            uint8_t g : 4;  ///< Green channel current (0x0-0xF)
            uint8_t b : 4;  ///< Blue channel current (0x0-0xF)
            uint8_t w : 4;  ///< White channel current (0x0-0xF)
        };
        uint16_t value;  ///< All channels as a single 16-bit value

        /// Default constructor - maximum brightness
        CurrentControl() : r(0xF), g(0xF), b(0xF), w(0xF) {}

        /// Construct from single brightness value (all channels)
        explicit CurrentControl(uint8_t brightness)
            : r(brightness & 0xF), g(brightness & 0xF), b(brightness & 0xF), w(brightness & 0xF) {}

        /// Construct from individual channel values
        CurrentControl(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t w_)
            : r(r_ & 0xF), g(g_ & 0xF), b(b_ & 0xF), w(w_ & 0xF) {}
    };

    /// @brief Set global UCS7604 brightness via current control (EXPERIMENTAL)
    /// @param current Current control settings for RGBW channels
    /// @note This is SECONDARY to FastLED::setBrightness() - use that as primary control
    /// @note Affects current control which may impact color accuracy
    void set_brightness(CurrentControl current);

    /// @brief Set global UCS7604 brightness with individual channel values (EXPERIMENTAL)
    /// @param r Red channel current (0x0-0xF)
    /// @param g Green channel current (0x0-0xF)
    /// @param b Blue channel current (0x0-0xF)
    /// @param w White channel current (0x0-0xF)
    /// @note This is SECONDARY to FastLED::setBrightness() - use that as primary control
    /// @note Affects current control which may impact color accuracy
    inline void set_brightness(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        set_brightness(CurrentControl(r, g, b, w));
    }

    /// @brief Get current global UCS7604 brightness value
    /// @return Current control settings
    CurrentControl brightness();

}  // namespace ucs7604

/// @brief UCS7604 protocol configuration modes
enum UCS7604Mode {
    UCS7604_MODE_8BIT_800KHZ = 0x03,
    UCS7604_MODE_16BIT_800KHZ = 0x8B,
    UCS7604_MODE_16BIT_1600KHZ = 0x9B // not implemented because of timing difference.
};




/// @brief UCS7604 controller extending CPixelLEDController
template <
    int DATA_PIN,
    EOrder RGB_ORDER,  // Color order for input pixels (converted to RGB internally)
    fl::UCS7604Mode MODE,
    typename CHIPSET_TIMING,
    template<int, typename, EOrder> class CLOCKLESS_CONTROLLER
>
class UCS7604ControllerT : public CPixelLEDController<RGB_ORDER>
{
private:
    using DelegateController = CLOCKLESS_CONTROLLER<DATA_PIN, CHIPSET_TIMING, RGB>;
    DelegateController mDelegate;  // Clockless controller for wire transmission (always RGB)

    static constexpr uint8_t PREAMBLE_LEN = 15;
    static constexpr uint8_t PREAMBLE_PIXELS = 5;  // 15 ÷ 3 = 5 exactly!

    // Reusable byte buffer (uses PSRAM on ESP32, regular heap elsewhere)
    // Cleared each frame but memory is reused (no reallocation after first use)
    fl::vector_psram<uint8_t> mByteBuffer;

public:
    UCS7604ControllerT() {}

    virtual void init() override {
        mDelegate.init();
    }

    // Access delegate controller (for testing)
    const DelegateController& getDelegate() const {
        return mDelegate;
    }

    DelegateController& getDelegate() {
        return mDelegate;
    }

protected:

    fl::span<const uint8_t> bytes() const {
        return mByteBuffer;
    }

    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        if (pixels.size() == 0) {
            return;
        }

        // Get current control values (recalculated each frame)
        fl::ucs7604::CurrentControl current = fl::ucs7604::brightness();

        // Reorder RGB current values to match the color order (RGB_ORDER template parameter)
        // The current control values are semantic (current.r controls RED LEDs, etc.)
        // but need to be sent in wire order matching the pixel data reordering
        uint8_t rgb_currents[3] = {current.r, current.g, current.b};

        // Extract channel positions from RGB_ORDER (octal encoding: 0=R, 1=G, 2=B)
        // Octal digits are read right-to-left in bit positions but left-to-right semantically
        // RGB (012 octal): wire position 0=R(0), 1=G(1), 2=B(2)
        // GRB (102 octal): wire position 0=G(1), 1=R(0), 2=B(2)
        uint8_t pos0 = (RGB_ORDER >> 6) & 0x3;  // Wire position 0 (leftmost octal digit)
        uint8_t pos1 = (RGB_ORDER >> 3) & 0x3;  // Wire position 1 (middle octal digit)
        uint8_t pos2 = (RGB_ORDER >> 0) & 0x3;  // Wire position 2 (rightmost octal digit)

        // Reorder: wire R gets current for channel at pos0, etc.
        uint8_t r_current = rgb_currents[pos0];  // Wire R (position 0)
        uint8_t g_current = rgb_currents[pos1];  // Wire G (position 1)
        uint8_t b_current = rgb_currents[pos2];  // Wire B (position 2)
        uint8_t w_current = current.w;  // W always in position 3

        // Step 1: Convert to PixelIterator with RGBW support
        //fl::Rgbw rgbw(fl::kRGBWDefaultColorTemp, fl::kRGBWExactColors, WDefault);
        fl::Rgbw rgbw = mDelegate.getRgbw();
        fl::PixelIterator pixel_iter = pixels.as_iterator(rgbw);

        // Calculate bytes per LED based on mode and RGB/RGBW
        size_t bytes_per_led;
        if (MODE == fl::UCS7604_MODE_8BIT_800KHZ) {
            bytes_per_led = pixel_iter.get_rgbw().active() ? 4 : 3;  // RGBW or RGB
        } else {
            bytes_per_led = pixel_iter.get_rgbw().active() ? 8 : 6;  // 16-bit RGBW or RGB
        }

        // Calculate total data size
        size_t led_data_size = pixels.size() * bytes_per_led;
        size_t total_data_size = PREAMBLE_LEN + led_data_size;

        // Calculate padding needed (0, 1, or 2 bytes)
        size_t padding = (3 - (total_data_size % 3)) % 3;

        // Clear buffer and reserve space
        mByteBuffer.clear();
        mByteBuffer.reserve(padding + total_data_size);

        // Step 2: Build preamble (15 bytes)
        buildPreamble(&mByteBuffer, r_current, g_current, b_current, w_current);

        // Step 3: Pre-pad with zeros if needed (goes to last LED)
        for (size_t i = 0; i < padding; ++i) {
            mByteBuffer.push_back(0);
        }

        // Step 4: Append LED data

        if (MODE == fl::UCS7604_MODE_8BIT_800KHZ) {
            // 8-bit mode: Use writeWS2812() which outputs RGB or RGBW
            pixel_iter.writeWS2812(&mByteBuffer);
        } else {
            // 16-bit mode: Apply gamma correction (power 2.8) to expand 8-bit to 16-bit
            if (pixel_iter.get_rgbw().active()) {
                // RGBW mode: 8 bytes per pixel (R16, G16, B16, W16)
                // Apply gamma correction (power 2.8) for 16-bit output
                while (pixel_iter.has(1)) {
                    uint8_t r, g, b, w;
                    pixel_iter.loadAndScaleRGBW(&r, &g, &b, &w);
                    pixel_iter.advanceData();

                    uint16_t r16 = fl::gamma_2_8(r);
                    uint16_t g16 = fl::gamma_2_8(g);
                    uint16_t b16 = fl::gamma_2_8(b);
                    uint16_t w16 = fl::gamma_2_8(w);

                    mByteBuffer.push_back(r16 >> 8);
                    mByteBuffer.push_back(r16 & 0xFF);
                    mByteBuffer.push_back(g16 >> 8);
                    mByteBuffer.push_back(g16 & 0xFF);
                    mByteBuffer.push_back(b16 >> 8);
                    mByteBuffer.push_back(b16 & 0xFF);
                    mByteBuffer.push_back(w16 >> 8);
                    mByteBuffer.push_back(w16 & 0xFF);
                }
            } else {
                // RGB mode: 6 bytes per pixel (R16, G16, B16)
                // Apply gamma correction (power 2.8) for 16-bit output
                while (pixel_iter.has(1)) {
                    uint8_t r, g, b;
                    pixel_iter.loadAndScaleRGB(&r, &g, &b);
                    pixel_iter.advanceData();

                    uint16_t r16 = fl::gamma_2_8(r);
                    uint16_t g16 = fl::gamma_2_8(g);
                    uint16_t b16 = fl::gamma_2_8(b);

                    mByteBuffer.push_back(r16 >> 8);
                    mByteBuffer.push_back(r16 & 0xFF);
                    mByteBuffer.push_back(g16 >> 8);
                    mByteBuffer.push_back(g16 & 0xFF);
                    mByteBuffer.push_back(b16 >> 8);
                    mByteBuffer.push_back(b16 & 0xFF);
                }
            }
        }
        
        // Step 4: Reinterpret byte buffer as CRGB pixels (now divisible by 3)
        size_t num_pixels = mByteBuffer.size() / 3;
        CRGB* fake_pixels = reinterpret_cast<CRGB*>(mByteBuffer.data());
        
        // Step 5: Construct PixelController and send to delegate controller
        PixelController<RGB> pixel_data(fake_pixels, num_pixels, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        mDelegate.showPixels(pixel_data);
    }

    


    static FASTLED_FORCE_INLINE void buildPreamble(fl::vector_psram<uint8_t>* buffer,
                                                     uint8_t r_current, uint8_t g_current,
                                                     uint8_t b_current, uint8_t w_current) {
        // Append 15-byte preamble to buffer
        buffer->push_back(0xFF);
        buffer->push_back(0xFF);
        buffer->push_back(0xFF);
        buffer->push_back(0xFF);
        buffer->push_back(0xFF);
        buffer->push_back(0xFF);
        buffer->push_back(0x00);
        buffer->push_back(0x02);
        buffer->push_back(static_cast<uint8_t>(MODE));
        buffer->push_back(r_current & 0x0F);
        buffer->push_back(g_current & 0x0F);
        buffer->push_back(b_current & 0x0F);
        buffer->push_back(w_current & 0x0F);
        buffer->push_back(0x00);
        buffer->push_back(0x00);
    }
};

// now typedef the controllers
template <int DATA_PIN, EOrder RGB_ORDER, template<int, typename, EOrder> class CLOCKLESS_CONTROLLER>
using UCS7604Controller8bitT = UCS7604ControllerT<DATA_PIN, RGB_ORDER, fl::UCS7604_MODE_8BIT_800KHZ, fl::TIMING_UCS7604_800KHZ, CLOCKLESS_CONTROLLER>;

template <int DATA_PIN, EOrder RGB_ORDER, template<int, typename, EOrder> class CLOCKLESS_CONTROLLER>
using UCS7604Controller16bitT = UCS7604ControllerT<DATA_PIN, RGB_ORDER, fl::UCS7604_MODE_16BIT_800KHZ, fl::TIMING_UCS7604_800KHZ, CLOCKLESS_CONTROLLER>;

template <int DATA_PIN, EOrder RGB_ORDER, template<int, typename, EOrder> class CLOCKLESS_CONTROLLER>
using UCS7604Controller16bit1600T = UCS7604ControllerT<DATA_PIN, RGB_ORDER, fl::UCS7604_MODE_16BIT_1600KHZ, fl::TIMING_UCS7604_1600KHZ, CLOCKLESS_CONTROLLER>;


}  // namespace fl

#endif // __INC_UCS7604_H
