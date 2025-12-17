
/// @reddit: reddit.com/u/ZachVorhies

#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "This file is only for ESP32-S3"
#endif

#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "fl/log.h"
#include "fl/stl/vector.h"
#include "pixel_iterator.h"

#ifndef FASTLED_INTERNAL
// We need to do a check for the esp-idf version because very specific versions
// of the esp-idf arduino core are broken. NOTE: ESP-IDF 5.1.0 itself should
// work with the legacy struct fields (psram_trans_align/sram_trans_align), but
// specific Arduino core releases had issues. If you're using plain
// ESP-IDF 5.1.0, you may be able to compile by defining FASTLED_INTERNAL to
// bypass this check.
#include "platforms/esp/esp_version.h"
// The struct fields changed in ESP-IDF 5.3.0 from
// psram_trans_align/sram_trans_align to dma_burst_size. The driver
// implementation handles both correctly. However, certain Arduino ESP32 core
// versions had other compatibility issues:
// - Broken in Arduino core 3.0.2, 3.0.4, 3.0.7 (esp-idf 5.1.x): "whole display
// same color"
// - Broken in Arduino core 3.1.0 (esp-idf 5.3.2): compatibility issues
// - Working in Arduino core 3.2.0+ (esp-idf 5.4.0+)
//
// If you're using ESP-IDF 5.1.0 directly (not through a broken Arduino core
// version), you can define FASTLED_INTERNAL to bypass this check at your own
// risk.
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 1, 0) &&                          \
    ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
#warning                                                                       \
    "I2S driver is known to not be compatible with ESP-IDF 5.1.x in certain Arduino core versions (3.0.2, 3.0.4, 3.0.7). Consider upgrading to ESP-IDF 5.4.0+ in Arduino core esp32 3.2.0+. See https://github.com/FastLED/FastLED/issues/1903"
#elif ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 3, 2)
#error                                                                         \
    "I2S driver is known to not be compatible with ESP-IDF 5.3.2, upgrade to ESP-IDF 5.4.0 in Arduino core esp32 3.2.0+, see https://github.com/FastLED/FastLED/issues/1903"
#endif
#endif // FASTLED_INTERNAL

namespace fl {

/// @brief Internal driver interface for I2S_Esp32. Use this
class InternalI2SDriver {
  public:
    static InternalI2SDriver *create();
    virtual ~InternalI2SDriver() {};
    virtual void
    initled(uint8_t *led_block,
            const int *pins,      // array of ints representing the gpio pins.
            int number_of_strips, // the number of strips, also describes the
                                  // size of the pins array.
            int number_of_leds_per_strip) = 0;
    virtual void setBrightness(uint8_t brightness) = 0;
    virtual void show() = 0;
};

class I2S_Esp32 {
  public:
    void beginShowLeds(int data_pin, int nleds, bool is_rgbw);
    void showPixels(uint8_t data_pin, PixelIterator &pixel_iterator);
    void endShowLeds();
};

// Base version of this class allows dynamic pins.
template <EOrder RGB_ORDER = RGB>
class ClocklessController_I2S_Esp32_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    I2S_Esp32 mI2S_Esp32;
    int mPin;

  public:
    ClocklessController_I2S_Esp32_WS2812Base(int pin) : mPin(pin) {}
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mI2S_Esp32.beginShowLeds(mPin, nleds, this->getRgbw().active());
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mI2S_Esp32.showPixels(mPin, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mI2S_Esp32.endShowLeds();
    }
};

// Same thing as above, but with a template parameter for the data pin so that
// it conforms to the API.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_I2S_Esp32_WS2812
    : public ClocklessController_I2S_Esp32_WS2812Base<RGB_ORDER> {
  private:
    typedef ClocklessController_I2S_Esp32_WS2812Base<RGB_ORDER> Base;
    I2S_Esp32 mI2S_Esp32;

    // Compile-time check for invalid pins
    static_assert(!(DATA_PIN == 19 || DATA_PIN == 20),
                  "GPIO19 and GPIO20 are reserved for USB-JTAG on ESP32S3 and "
                  "cannot be used for LED output. "
                  "Using these pins will break USB flashing capability. Please "
                  "choose a different pin.");

  public:
    ClocklessController_I2S_Esp32_WS2812() : Base(DATA_PIN) {};
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }
};

} // namespace fl
