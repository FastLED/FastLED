#pragma once

#ifndef __INC_UCS7604_H
#define __INC_UCS7604_H

#include "pixeltypes.h"
#include "pixel_controller.h"
#include "cpixel_ledcontroller.h"
#include "fl/force_inline.h"
#include "platforms/shared/clockless_blocking.h"
#include "fl/chipsets/led_timing.h"
#include "fl/pixel_iterator.h"
#include "fl/vector.h"
#include "lib8tion/intmap.h"
#include "fl/type_traits.h"

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

namespace fl {

/// @brief UCS7604 protocol configuration modes
enum UCS7604Mode {
    UCS7604_MODE_8BIT_800KHZ = 0x03,
    UCS7604_MODE_16BIT_800KHZ = 0x8B,
    UCS7604_MODE_16BIT_1600KHZ = 0x9B // not implemented because of timing difference.
};




/// @brief UCS7604 controller extending ClocklessBlockingGeneric
template <
    int DATA_PIN,
    EOrder RGB_ORDER,
    fl::UCS7604Mode MODE, // = fl::UCS7604_MODE_8BIT_800KHZ,
    typename CHIPSET_TIMING, // = fl::WS2812ChipsetTiming
    template<int, typename, EOrder> class CLOCKLESS_CONTROLLER
>
class UCS7604Controller : public CLOCKLESS_CONTROLLER<DATA_PIN, CHIPSET_TIMING, RGB_ORDER>
{
private:
    using BaseController = CLOCKLESS_CONTROLLER<DATA_PIN, CHIPSET_TIMING, RGB_ORDER>;

    static constexpr uint8_t PREAMBLE_LEN = 15;
    static constexpr uint8_t PREAMBLE_PIXELS = 5;  // 15 ÷ 3 = 5 exactly!

    uint8_t mRCurrent;
    uint8_t mGCurrent;
    uint8_t mBCurrent;
    uint8_t mWCurrent;

    // Reusable byte buffer (uses PSRAM on ESP32, regular heap elsewhere)
    // Cleared each frame but memory is reused (no reallocation after first use)
    fl::vector_psram<uint8_t> mByteBuffer;

public:
    UCS7604Controller(uint8_t r_current = 0x0F, uint8_t g_current = 0x0F,
                      uint8_t b_current = 0x0F, uint8_t w_current = 0x0F)
        : mRCurrent(r_current & 0x0F)
        , mGCurrent(g_current & 0x0F)
        , mBCurrent(b_current & 0x0F)
        , mWCurrent(w_current & 0x0F)
    {}

    void setCurrentControl(uint8_t r_current, uint8_t g_current,
                          uint8_t b_current, uint8_t w_current) {
        mRCurrent = r_current & 0x0F;
        mGCurrent = g_current & 0x0F;
        mBCurrent = b_current & 0x0F;
        mWCurrent = w_current & 0x0F;
    }

protected:
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        if (pixels.size() == 0) {
            return;
        }

        // Step 1: Convert to PixelIterator with RGBW support
        //fl::Rgbw rgbw(fl::kRGBWDefaultColorTemp, fl::kRGBWExactColors, WDefault);
        fl::Rgbw rgbw = BaseController::getRgbw();
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
        buildPreamble(&mByteBuffer, mRCurrent, mGCurrent, mBCurrent, mWCurrent);

        // Step 3: Pre-pad with zeros if needed (goes to last LED)
        for (size_t i = 0; i < padding; ++i) {
            mByteBuffer.push_back(0);
        }

        // Step 4: Append LED data

        if (MODE == fl::UCS7604_MODE_8BIT_800KHZ) {
            // 8-bit mode: Use writeWS2812() which outputs RGB or RGBW
            pixel_iter.writeWS2812(&mByteBuffer);
        } else {
            // 16-bit mode: Expand 8-bit to 16-bit using fl::int_scale
            if (pixel_iter.get_rgbw().active()) {
                // RGBW mode: 8 bytes per pixel (R16, G16, B16, W16)
                while (pixel_iter.has(1)) {
                    uint8_t r, g, b, w;
                    pixel_iter.loadAndScaleRGBW(&r, &g, &b, &w);
                    pixel_iter.advanceData();

                    uint16_t r16 = fl::int_scale<uint8_t, uint16_t>(r);
                    uint16_t g16 = fl::int_scale<uint8_t, uint16_t>(g);
                    uint16_t b16 = fl::int_scale<uint8_t, uint16_t>(b);
                    uint16_t w16 = fl::int_scale<uint8_t, uint16_t>(w);

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
                while (pixel_iter.has(1)) {
                    uint8_t r, g, b;
                    pixel_iter.loadAndScaleRGB(&r, &g, &b);
                    pixel_iter.advanceData();

                    uint16_t r16 = fl::int_scale<uint8_t, uint16_t>(r);
                    uint16_t g16 = fl::int_scale<uint8_t, uint16_t>(g);
                    uint16_t b16 = fl::int_scale<uint8_t, uint16_t>(b);

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
        
        // Step 5: Construct PixelController and send to base controller
        ColorAdjustment no_adjust = {
            CRGB(255, 255, 255)  // premixed: No color correction
#if FASTLED_HD_COLOR_MIXING
            , CRGB(255, 255, 255)  // color: full scale
            , 255  // brightness: max
#endif
        };
        PixelController<RGB> pixel_data(fake_pixels, num_pixels, no_adjust, DISABLE_DITHER);
        BaseController::showPixels(pixel_data);
    }

private:
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
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
using UCS7604Controller8bit = UCS7604Controller<DATA_PIN, RGB_ORDER, fl::UCS7604_MODE_8BIT_800KHZ, fl::TIMING_UCS7604_800KHZ, fl::ClocklessBlockingGeneric>;

template <int DATA_PIN, EOrder RGB_ORDER = GRB>
using UCS7604Controller16bit = UCS7604Controller<DATA_PIN, RGB_ORDER, fl::UCS7604_MODE_16BIT_800KHZ, fl::TIMING_UCS7604_800KHZ, fl::ClocklessBlockingGeneric>;

template <int DATA_PIN, EOrder RGB_ORDER = GRB>
using UCS7604Controller16bit1600 = UCS7604Controller<DATA_PIN, RGB_ORDER, fl::UCS7604_MODE_16BIT_1600KHZ, fl::TIMING_UCS7604_1600KHZ, fl::ClocklessBlockingGeneric>;


}  // namespace fl

#endif // __INC_UCS7604_H
