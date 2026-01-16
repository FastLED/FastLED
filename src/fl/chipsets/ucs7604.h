#pragma once

#ifndef __INC_UCS7604_H
#define __INC_UCS7604_H

#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "pixel_controller.h"
#include "cpixel_ledcontroller.h"
#include "fl/force_inline.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/encoders/ucs7604.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/vector.h"
#include "lib8tion/intmap.h"
#include "fl/stl/type_traits.h"
#include "fl/ease.h"
#include "fl/stl/iterator.h"

/// @file fl/chipsets/ucs7604.h
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
    /// @brief Type alias for current control (defined in ucs7604_encoder.h)
    using CurrentControl = UCS7604CurrentControl;

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
    using DelegateControllerBase = CLOCKLESS_CONTROLLER<DATA_PIN, CHIPSET_TIMING, RGB>;

    // Helper class to access protected methods of the delegate controller
    class DelegateController : public DelegateControllerBase {
        friend class UCS7604ControllerT<DATA_PIN, RGB_ORDER, MODE, CHIPSET_TIMING, CLOCKLESS_CONTROLLER>;
        void callShowPixels(PixelController<RGB> & pixels) {
            DelegateControllerBase::showPixels(pixels);
        }
    };

    DelegateController mDelegate;  // Clockless controller for wire transmission (always RGB)

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

        // Create current control struct with reordered values
        fl::UCS7604CurrentControl wire_current(r_current, g_current, b_current, w_current);

        // Convert to PixelIterator with RGBW support
        fl::Rgbw rgbw = mDelegate.getRgbw();
        fl::PixelIterator pixel_iter = pixels.as_iterator(rgbw);

        // Clear buffer and use encoder to fill it
        mByteBuffer.clear();
        fl::encodeUCS7604(pixel_iter, pixels.size(), fl::back_inserter(mByteBuffer),
                          MODE, wire_current, pixel_iter.get_rgbw().active());

        // Reinterpret byte buffer as CRGB pixels (encoder ensures divisible by 3)
        size_t num_pixels = mByteBuffer.size() / 3;
        CRGB* fake_pixels = fl::bit_cast<CRGB*>(mByteBuffer.data());

        // Construct PixelController and send to delegate controller
        PixelController<RGB> pixel_data(fake_pixels, num_pixels, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        mDelegate.callShowPixels(pixel_data);
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
