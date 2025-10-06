/// @file clockless_lcd_i80_esp32.h
/// @brief ESP32 I80/LCD_CAM parallel LED driver wrapper
///
/// This file provides the FastLED controller interface for the I80 LCD driver.
/// The actual driver implementation is in lcd/lcd_driver_i80.h
///
/// Supported platforms:
/// - ESP32-S3: LCD_CAM peripheral with I80 interface
/// - ESP32-P4: I80 interface (if available)

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
#include "lcd/lcd_driver_i80.h"

namespace fl {

// Bring in LcdDriverConfig from common header
using fl::LcdDriverConfig;

}  // namespace fl

// Forward declarations for wrapper API (matches I2S driver pattern)
namespace fl {

/// @brief LCD_I80_Esp32 wrapper class that uses RectangularDrawBuffer
/// This provides the same interface as I2S_Esp32
/// Implementation is in clockless_lcd_i80_esp32.cpp (compiled for ESP32-S3)
class LCD_I80_Esp32 {
  public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

// Base version of this class allows dynamic pins (WS2812 chipset)
template <EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_I80_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    LCD_I80_Esp32 mLCD_I80_Esp32;
    int mPin;

  public:
    ClocklessController_LCD_I80_WS2812Base(int pin): mPin(pin) {}
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mLCD_I80_Esp32.beginShowLeds(mPin, nleds);
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mLCD_I80_Esp32.showPixels(mPin, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mLCD_I80_Esp32.endShowLeds();
    }
};


// Template parameter for the data pin so that it conforms to the API.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_LCD_I80_WS2812
    : public ClocklessController_LCD_I80_WS2812Base<RGB_ORDER> {
  private:
    typedef ClocklessController_LCD_I80_WS2812Base<RGB_ORDER> Base;

    // Compile-time check for invalid pins
    static_assert(!(DATA_PIN == 19 || DATA_PIN == 20),
                  "GPIO19 and GPIO20 are reserved for USB-JTAG on ESP32-S2/S3 and CANNOT be used for LED output. "
                  "Using these pins WILL BREAK USB flashing capability. Please choose a different pin.");

    static_assert(!(DATA_PIN >= 26 && DATA_PIN <= 32),
                  "GPIO26-32 are reserved for SPI Flash/PSRAM and CANNOT be used for LED output. "
                  "Using these pins WILL BREAK flash/PSRAM functionality. Please choose a different pin.");

    // Warning for strapping pins (compile-time warning via static_assert with always-true condition)
    static_assert(!(DATA_PIN == 0 || DATA_PIN == 3 || DATA_PIN == 45 || DATA_PIN == 46) || true,
                  "WARNING: GPIO0, GPIO3, GPIO45, and GPIO46 are strapping pins used for boot configuration. "
                  "Using these pins may affect boot behavior and requires careful external circuit design.");

    #if defined(CONFIG_SPIRAM_MODE_OCT) || defined(CONFIG_ESPTOOLPY_FLASHMODE_OPI)
    static_assert(!(DATA_PIN >= 33 && DATA_PIN <= 37),
                  "GPIO33-37 are reserved for Octal Flash/PSRAM (SPIIO4-7, SPIDQS) and CANNOT be used for LED output. "
                  "Using these pins WILL BREAK Octal flash/PSRAM functionality. Please choose a different pin.");
    #endif

  public:
    ClocklessController_LCD_I80_WS2812(): Base(DATA_PIN) {};
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

};

} // namespace fl
