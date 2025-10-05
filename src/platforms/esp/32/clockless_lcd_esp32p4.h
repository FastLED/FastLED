/// @file clockless_lcd_esp32p4.h
/// @brief ESP32-P4 RGB LCD parallel LED driver wrapper
///
/// This file provides the FastLED controller interface for the ESP32-P4 LCD driver.
/// The actual driver implementation is in lcd/lcd_driver_p4.h
///
/// Supported platforms:
/// - ESP32-P4: RGB LCD peripheral

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "sdkconfig.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"

// Include the P4 LCD driver
#include "lcd/lcd_driver_p4.h"

namespace fl {

// Bring in LcdP4DriverConfig from P4 driver header
using fl::LcdP4DriverConfig;

}  // namespace fl

// Forward declarations for wrapper API (matches I2S/S3 LCD driver pattern)
namespace fl {

/// @brief LCD_Esp32P4 wrapper class that uses RectangularDrawBuffer
/// This provides the same interface as I2S_Esp32 and LCD_Esp32
/// Implementation is in clockless_lcd_esp32p4.cpp (always compiled for ESP32-P4)
class LCD_Esp32P4 {
  public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

// Base version of this class allows dynamic pins (WS2812 chipset)
template <EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_Esp32P4_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    LCD_Esp32P4 mLCD_Esp32P4;
    int mPin;

  public:
    ClocklessController_LCD_Esp32P4_WS2812Base(int pin): mPin(pin) {}
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mLCD_Esp32P4.beginShowLeds(mPin, nleds);
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mLCD_Esp32P4.showPixels(mPin, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mLCD_Esp32P4.endShowLeds();
    }
};


// Template parameter for the data pin so that it conforms to the API.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_Esp32P4_WS2812
    : public ClocklessController_LCD_Esp32P4_WS2812Base<RGB_ORDER> {
  private:
    typedef ClocklessController_LCD_Esp32P4_WS2812Base<RGB_ORDER> Base;

    // Compile-time check for invalid pins (P4-specific)
    // TODO: Add P4-specific pin validation based on datasheet

  public:
    ClocklessController_LCD_Esp32P4_WS2812(): Base(DATA_PIN) {};
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

};

} // namespace fl
