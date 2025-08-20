#pragma once

/// @file clockless.h
/// Adafruit_NeoPixel-based clockless controller implementation
/// 
/// This header provides a FastLED-compatible clockless controller that uses
/// the Adafruit_NeoPixel library as the underlying driver. This allows users
/// to leverage the proven reliability and platform-specific optimizations
/// of the Adafruit library within the FastLED ecosystem.
///
/// Requirements:
/// - Adafruit_NeoPixel library must be included before FastLED
/// - The controller is only available when Adafruit_NeoPixel.h is detected

#ifndef FASTLED_USE_ADAFRUIT_NEOPIXEL
#ifdef FASTLED_DOXYGEN
#define FASTLED_USE_ADAFRUIT_NEOPIXEL 1
#else
#define FASTLED_USE_ADAFRUIT_NEOPIXEL 0
#endif
#endif



#if FASTLED_USE_ADAFRUIT_NEOPIXEL && !__has_include("Adafruit_NeoPixel.h")
#error "Adafruit_NeoPixel.h not found, disabling FastLED_USE_ADAFRUIT_NEOPIXEL"
#define FASTLED_USE_ADAFRUIT_NEOPIXEL 0
#endif

#if FASTLED_USE_ADAFRUIT_NEOPIXEL

#include "fl/namespace.h"
#include "fl/memory.h"
#include "controller.h"
#include "pixel_controller.h"
#include "pixel_iterator.h"
#include "color.h"
#include "fastled_config.h"

namespace fl {

// Forward declaration - implementation in clockless.cpp
class AdafruitNeoPixelDriver {
private:
    class Impl;
    fl::unique_ptr<Impl> pImpl;
    
public:
    AdafruitNeoPixelDriver();
    ~AdafruitNeoPixelDriver();
    void init(int dataPin, int numPixels);
    void showPixels(PixelIterator& pixelIterator);
    void updateLength(int numPixels);
    int getPixelCount() const;
    bool isInitialized() const;
};

/// WS2812/NeoPixel clockless controller using Adafruit_NeoPixel library as the underlying driver
/// 
/// This controller provides a simple interface for WS2812/NeoPixel LEDs using the proven
/// Adafruit_NeoPixel library for platform-specific optimizations and reliable output.
/// Supports RGB color order conversion and handles initialization, pixel conversion,
/// and output automatically.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam RGB_ORDER the RGB ordering for the LEDs (affects input processing, output is always RGB)
/// @see https://github.com/adafruit/Adafruit_NeoPixel
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
private:
    AdafruitNeoPixelDriver mDriver;

public:
    /// Constructor - creates uninitialized controller
    ClocklessController() = default;
    
    /// Destructor - automatic cleanup
    virtual ~ClocklessController() = default;

    /// Initialize the controller
    virtual void init() override {
        // Driver will be initialized when showPixels is first called
    }

    /// Output pixels to the LED strip
    /// Converts FastLED pixel data to Adafruit format and displays
    /// @param pixels the pixel controller containing LED data
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        // Initialize driver if needed
        if (!mDriver.isInitialized()) {
            mDriver.init(DATA_PIN, pixels.size());
        } else if (mDriver.getPixelCount() != pixels.size()) {
            mDriver.updateLength(pixels.size());
        }
        
        // Convert to PixelIterator and send to driver
        auto pixelIterator = pixels.as_iterator(RgbwInvalid());
        mDriver.showPixels(pixelIterator);
    }

protected:
    /// Get the driver instance (for derived classes)
    AdafruitNeoPixelDriver& getDriver() {
        return mDriver;
    }
    
    /// Check if the controller is initialized
    bool isInitialized() const {
        return mDriver.isInitialized();
    }
};



} // namespace fl

#endif  // FASTLED_USE_ADAFRUIT_NEOPIXEL
