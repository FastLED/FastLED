/// @file clockless_lcd_i80_esp32.h
/// @brief ESP32 I80/LCD_CAM parallel LED driver wrapper
///
/// This file provides the FastLED controller interface for the I80 LCD driver.
/// The actual driver implementation is in lcd/lcd_driver_i80.h
///
/// Supported platforms:
/// - ESP32-S3: LCD_CAM peripheral with I80 interface
/// - ESP32-P4: I80 interface (if available)
///
/// **Supported Chipsets:**
/// The underlying LcdI80Driver template is fully generic and supports multiple LED chipsets
/// via compile-time T1/T2/T3 timing calculation. See clockless_lcd_i80_esp32.cpp for the
/// list of instantiated chipsets (WS2812, SK6812, WS2813, TM1829, etc.).
///
/// **Current Limitation:**
/// The wrapper controller (ClocklessController_LCD_I80_WS2812) currently uses a singleton
/// pattern that only supports WS2812 timing at runtime. To use other chipsets, the wrapper
/// would need to be refactored to support multiple driver instances or template-based chipset
/// selection. The low-level driver already supports this - it's just the high-level wrapper
/// that needs updating.

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-S3 and ESP32-P4 (I80 LCD interface required)"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S2)
#error "LCD driver is not supported on ESP32-S2"
#endif

#include "sdkconfig.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"

// Include the I80 LCD driver
#include "lcd_driver_i80.h"
#include "lcd_i80_registry.h"
#include "fl/chipsets/led_timing.h"

namespace fl {

// Bring in LcdDriverConfig from common header
using fl::LcdDriverConfig;

}  // namespace fl

// Forward declarations for wrapper API (matches I2S driver pattern)
namespace fl {

// Forward declare the templated group (defined in .cpp file)
template <typename TIMING> class LCDI80Esp32Group;

/// @brief LCD_I80_Esp32 wrapper class that uses RectangularDrawBuffer
/// This provides the same interface as I2S_Esp32
/// Implementation is in clockless_lcd_i80_esp32.cpp (compiled for ESP32-S3)
/// @note This is a temporary compatibility layer - new code should use ClocklessController_LCD_I80_Proxy<TIMING>
class LCD_I80_Esp32 {
  public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

/// @brief Generic proxy controller for LCD I80 on ESP32-S3/P4
/// Works with ANY clockless chipset timing (WS2812, SK6812, WS2813, etc.)
///
/// Automatically routes to the correct LCDI80Esp32Group<TIMING> singleton
/// based on the TIMING template parameter. Multiple chipsets can coexist.
///
/// @tparam TIMING LED chipset timing struct (e.g., TIMING_WS2812_800KHZ, TIMING_SK6812)
/// @tparam DATA_PIN GPIO pin number for this strip
/// @tparam RGB_ORDER Color ordering (RGB, GRB, etc.)
template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_I80_Proxy
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;

    // Compile-time GPIO validation (same as before)
    static_assert(!(DATA_PIN == 19 || DATA_PIN == 20),
                  "GPIO19 and GPIO20 are reserved for USB-JTAG and CANNOT be used");
    static_assert(!(DATA_PIN >= 26 && DATA_PIN <= 32),
                  "GPIO26-32 are reserved for Flash/PSRAM and CANNOT be used");
    static_assert(!(DATA_PIN == 0 || DATA_PIN == 3 || DATA_PIN == 45 || DATA_PIN == 46) || true,
                  "WARNING: Strapping pins - use with caution");
    #if defined(CONFIG_SPIRAM_MODE_OCT) || defined(CONFIG_ESPTOOLPY_FLASHMODE_OPI)
    static_assert(!(DATA_PIN >= 33 && DATA_PIN <= 37),
                  "GPIO33-37 reserved for Octal Flash/PSRAM");
    #endif

  public:
    ClocklessController_LCD_I80_Proxy() : Base() {}

    void init() override {}

    virtual uint16_t getMaxRefreshRate() const override {
        // Calculate based on timing
        uint32_t total_ns = TIMING::T1 + TIMING::T2 + TIMING::T3;
        return (total_ns > 2000) ? 400 : 800;
    }

  protected:
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);

        // Auto-grab the singleton for THIS chipset type
        LCDI80Esp32Group<TIMING>& group = LCDI80Esp32Group<TIMING>::getInstance();

        // Flush any pending groups of DIFFERENT chipset types
        LCDI80Esp32Registry::getInstance().flushAllExcept(&group);

        // Start queuing for this group
        group.onQueuingStart();
        group.addObject(DATA_PIN, nleds, false);

        return data;
    }

    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        // Auto-grab the singleton for THIS chipset type
        LCDI80Esp32Group<TIMING>& group = LCDI80Esp32Group<TIMING>::getInstance();

        group.onQueuingDone();

        // Copy pixels to buffer
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        const Rgbw rgbw = pixel_iterator.get_rgbw();

        fl::span<uint8_t> strip_bytes = group.mBase.mRectDrawBuffer.getLedsBufferBytesForPin(DATA_PIN, true);

        if (rgbw.active()) {
            uint8_t r, g, b, w;
            while (pixel_iterator.has(1)) {
                pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
                strip_bytes[0] = r;
                strip_bytes[1] = g;
                strip_bytes[2] = b;
                strip_bytes[3] = w;
                strip_bytes.pop_front();
                strip_bytes.pop_front();
                strip_bytes.pop_front();
                strip_bytes.pop_front();
                pixel_iterator.advanceData();
                pixel_iterator.stepDithering();
            }
        } else {
            uint8_t r, g, b;
            while (pixel_iterator.has(1)) {
                pixel_iterator.loadAndScaleRGB(&r, &g, &b);
                strip_bytes[0] = r;
                strip_bytes[1] = g;
                strip_bytes[2] = b;
                strip_bytes.pop_front();
                strip_bytes.pop_front();
                strip_bytes.pop_front();
                pixel_iterator.advanceData();
                pixel_iterator.stepDithering();
            }
        }
    }

    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);

        // DON'T flush here - let chipset change or frame end handle it
        // Registry will flush on chipset change or explicit call
        LCDI80Esp32Group<TIMING>& group = LCDI80Esp32Group<TIMING>::getInstance();
        group.showPixelsOnceThisFrame();
    }
};

// Backward-compatible alias for WS2812 (existing code compatibility)
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
using ClocklessController_LCD_I80_WS2812 =
    ClocklessController_LCD_I80_Proxy<TIMING_WS2812_800KHZ, DATA_PIN, RGB_ORDER>;

} // namespace fl
