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


#include "fl/memory.h"
#include "fl/unique_ptr.h"
#include "eorder.h"
#include "pixel_controller.h"

class PixelIterator;

namespace fl {

/// Interface for Adafruit NeoPixel driver - implementation in clockless.cpp
class IAdafruitNeoPixelDriver {
public:
    /// Static factory method to create driver implementation
    static fl::unique_ptr<IAdafruitNeoPixelDriver> create();

    virtual ~IAdafruitNeoPixelDriver() = default;
    
    /// Initialize the driver with data pin and RGBW mode
    virtual void init(int dataPin) = 0;
    
    /// Output pixels to the LED strip
    virtual void showPixels(PixelIterator& pixelIterator) = 0;

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
    fl::unique_ptr<IAdafruitNeoPixelDriver> mDriver;

public:
    /// Constructor - creates uninitialized controller
    ClocklessController(){}
    
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
        if (!mDriver) {
            mDriver = IAdafruitNeoPixelDriver::create();
            mDriver->init(DATA_PIN);
        }        
        // Convert to PixelIterator and send to driver
        auto pixelIterator = pixels.as_iterator(this->getRgbw());
        mDriver->showPixels(pixelIterator);
    }

protected:
    /// Get the driver instance (for derived classes)
    IAdafruitNeoPixelDriver& getDriver() {
        return *mDriver;
    }
};



} // namespace fl

#endif  // FASTLED_USE_ADAFRUIT_NEOPIXEL
