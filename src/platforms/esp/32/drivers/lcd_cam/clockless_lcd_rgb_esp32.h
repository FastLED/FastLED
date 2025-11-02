/// @file clockless_lcd_rgb_esp32.h
/// @brief ESP32 RGB LCD parallel LED driver wrapper
///
/// This file provides the FastLED controller interface for the RGB LCD driver.
/// The actual driver implementation is in lcd/lcd_driver_rgb.h
///
/// Supported platforms:
/// - ESP32-P4: RGB LCD peripheral
/// - Future ESP32 variants with RGB LCD support

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "sdkconfig.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"

// Include the RGB LCD driver
#include "lcd_driver_rgb.h"

namespace fl {

// Bring in LcdRgbDriverConfig from RGB driver header
using fl::LcdRgbDriverConfig;

}  // namespace fl

// Forward declarations for wrapper API (matches I2S/S3 LCD driver pattern)
namespace fl {

/// @brief LCD_RGB_Esp32 wrapper class that uses RectangularDrawBuffer
/// This provides the same interface as I2S_Esp32 and LCD_I80_Esp32
/// Implementation is in clockless_lcd_rgb_esp32.cpp (compiled for ESP32-P4)
class LCD_RGB_Esp32 {
  public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

// Base version of this class allows dynamic pins (WS2812 chipset)
template <EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_RGB_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    LCD_RGB_Esp32 mLCD_RGB_Esp32;
    int mPin;

  public:
    ClocklessController_LCD_RGB_WS2812Base(int pin): mPin(pin) {}
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mLCD_RGB_Esp32.beginShowLeds(mPin, nleds);
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mLCD_RGB_Esp32.showPixels(mPin, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mLCD_RGB_Esp32.endShowLeds();
    }
};


// Template parameter for the data pin so that it conforms to the API.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_RGB_WS2812
    : public ClocklessController_LCD_RGB_WS2812Base<RGB_ORDER> {
  private:
    typedef ClocklessController_LCD_RGB_WS2812Base<RGB_ORDER> Base;

    // Compile-time check for invalid pins (P4-specific)
    // TODO: Add P4-specific pin validation based on datasheet

  public:
    ClocklessController_LCD_RGB_WS2812(): Base(DATA_PIN) {};
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

};

} // namespace fl
