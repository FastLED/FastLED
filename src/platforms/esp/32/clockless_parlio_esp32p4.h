/// @file clockless_parlio_esp32p4.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED driver for simultaneous multi-strip output
///
/// This driver uses the ESP32-P4 PARLIO TX peripheral to drive up to 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Supported platforms:
/// - ESP32-P4: PARLIO TX peripheral (requires driver/parlio_tx.h)
///
/// Key features:
/// - Simultaneous output to multiple LED strips
/// - DMA-based transmission (minimal CPU overhead)
/// - Hardware timing control (no CPU bit-banging)
/// - High performance (+ FPS for 256-pixel strips)

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "sdkconfig.h"

#include <stdint.h>
#include "platforms/esp/esp_version.h"

#include "crgb.h"
#include "cpixel_ledcontroller.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "platforms/shared/clockless_timing.h"
#include "fl/namespace.h"

// Include the PARLIO driver
#include "parlio/parlio_driver.h"

namespace fl {

// ===== Proxy Controller System (matches I2S pattern) =====

/// @brief Helper object for PARLIO proxy controllers
class Parlio_Esp32P4 {
public:
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

/// @brief Base proxy controller with dynamic pin
template <EOrder RGB_ORDER = RGB>
class ClocklessController_Parlio_Esp32P4_WS2812Base
    : public CPixelLEDController<RGB_ORDER> {
private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    Parlio_Esp32P4 mParlio_Esp32P4;
    int mPin;

public:
    ClocklessController_Parlio_Esp32P4_WS2812Base(int pin) : mPin(pin) {}

    void init() override {}

    uint16_t getMaxRefreshRate() const override { return 800; }

protected:
    void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mParlio_Esp32P4.beginShowLeds(mPin, nleds);
        return data;
    }

    void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mParlio_Esp32P4.showPixels(mPin, pixel_iterator);
    }

    void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mParlio_Esp32P4.endShowLeds();
    }
};

/// @brief Template version with compile-time pin
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_Parlio_Esp32P4_WS2812
    : public ClocklessController_Parlio_Esp32P4_WS2812Base<RGB_ORDER> {
private:
    typedef ClocklessController_Parlio_Esp32P4_WS2812Base<RGB_ORDER> Base;

public:
    ClocklessController_Parlio_Esp32P4_WS2812() : Base(DATA_PIN) {}

    void init() override {}

    uint16_t getMaxRefreshRate() const override { return 800; }
};

}  // namespace fl
