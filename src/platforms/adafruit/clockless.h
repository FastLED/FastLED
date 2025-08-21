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


#include "fl/memory.h"
#include "fl/unique_ptr.h"
#include "eorder.h"
#include "pixel_controller.h"
#include "platforms/adafruit/driver.h"


namespace fl {
class PixelIterator;

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
class AdafruitWS2812Controller : public CPixelLEDController<RGB_ORDER> {
private:
    fl::unique_ptr<fl::IAdafruitNeoPixelDriver> mDriver;

public:
    /// Constructor - creates uninitialized controller
    AdafruitWS2812Controller(){}
    
    /// Destructor - automatic cleanup
    virtual ~AdafruitWS2812Controller() = default;

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
            mDriver = fl::IAdafruitNeoPixelDriver::create();
            mDriver->init(DATA_PIN);
        }        
        // Convert to PixelIterator and send to driver
        auto pixelIterator = pixels.as_iterator(this->getRgbw());
        mDriver->showPixels(pixelIterator);
    }

protected:
    /// Get the driver instance (for derived classes)
    fl::IAdafruitNeoPixelDriver& getDriver() {
        return *mDriver;
    }
};
}  // namespace fl
