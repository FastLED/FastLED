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

#include "Adafruit_NeoPixel.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "controller.h"
#include "pixel_controller.h"
#include "color.h"
#include "fastled_config.h"

namespace fl {  

/// WS2812 controller using Adafruit_NeoPixel as the underlying driver
/// 
/// This controller provides WS2812/NeoPixel support through the Adafruit_NeoPixel
/// library, leveraging its platform-specific optimizations and proven reliability.
/// Supports RGB color order conversion and handles initialization, pixel conversion,
/// and output automatically.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam RGB_ORDER the RGB ordering for the LEDs (affects input processing, output to Adafruit is always RGB)
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class AdafruitWS2812Controller : public CPixelLEDController<RGB_ORDER> {
private:
    fl::unique_ptr<Adafruit_NeoPixel> mNeoPixel;
    bool mInitialized;

public:
    /// Constructor - creates uninitialized controller
    AdafruitWS2812Controller() : mNeoPixel(nullptr), mInitialized(false) {}
    
    /// Destructor - automatic cleanup via unique_ptr
    virtual ~AdafruitWS2812Controller() = default;

    /// Initialize the controller
    /// Creates the Adafruit_NeoPixel instance with RGB color order
    /// Note: Always uses RGB since PixelController handles color ordering
    virtual void init() override {
        if (!mInitialized) {
            // Get pixel type configuration from derived class
            neoPixelType pixelType = getPixelType();
            
            // Create Adafruit_NeoPixel instance
            // Note: numPixels will be set when FastLED calls setLeds()
            mNeoPixel = fl::make_unique<Adafruit_NeoPixel>(0, DATA_PIN, pixelType);
            mInitialized = true;
            
            // Allow derived classes to perform additional initialization
            onInitialized();
        }
    }

    /// Output pixels to the LED strip
    /// Converts FastLED pixel data to Adafruit format and displays
    /// @param pixels the pixel controller containing LED data
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        if (!mNeoPixel || !mInitialized) {
            return;
        }
        
        // Update strip length if needed
        if (mNeoPixel->numPixels() != pixels.size()) {
            mNeoPixel->updateLength(pixels.size());
            mNeoPixel->begin();
        }
        
        // Allow derived classes to perform pre-show operations
        beforeShow(pixels);
        
        // Convert FastLED pixel data to Adafruit format
        // Note: PixelController handles color order conversion and always outputs RGB
        convertAndSetPixels(pixels);
        
        // Allow derived classes to perform post-conversion operations
        afterConversion(pixels);
        
        // Output to LEDs via Adafruit library
        mNeoPixel->show();
        
        // Allow derived classes to perform post-show operations
        afterShow(pixels);
    }

protected:
    /// Get the Adafruit_NeoPixel instance (for derived classes)
    Adafruit_NeoPixel* getNeoPixel() const {
        return mNeoPixel.get();
    }
    
    /// Check if the controller is initialized
    bool isInitialized() const {
        return mInitialized;
    }
    
    /// Get the pixel type configuration for the Adafruit_NeoPixel instance
    /// Override this in derived classes to customize pixel type
    /// @return neoPixelType configuration flags
    virtual neoPixelType getPixelType() const {
        // Default: RGB order with 800KHz timing
        // The RGB_ORDER template parameter affects PixelController processing,
        // but the output to Adafruit_NeoPixel is always RGB
        return NEO_RGB | NEO_KHZ800;
    }
    
    /// Called after successful initialization
    /// Override in derived classes for additional setup
    virtual void onInitialized() {
        // Default: no additional initialization
    }
    
    /// Called before showing pixels
    /// Override in derived classes for pre-show operations
    /// @param pixels the pixel controller containing LED data
    virtual void beforeShow(PixelController<RGB_ORDER> &pixels) {
        // Default: no pre-show operations
    }
    
    /// Convert and set pixels in the Adafruit_NeoPixel instance
    /// Override in derived classes to customize pixel conversion
    /// @param pixels the pixel controller containing LED data
    virtual void convertAndSetPixels(PixelController<RGB_ORDER> &pixels) {
        auto iterator = pixels.as_iterator(RgbwInvalid());
        for (int i = 0; iterator.has(1); ++i) {
            fl::u8 r, g, b;
            iterator.loadAndScaleRGB(&r, &g, &b);
            mNeoPixel->setPixelColor(i, r, g, b);
            iterator.advanceData();
        }
    }
    
    /// Called after pixel conversion but before show()
    /// Override in derived classes for post-conversion operations
    /// @param pixels the pixel controller containing LED data
    virtual void afterConversion(PixelController<RGB_ORDER> &pixels) {
        // Default: no post-conversion operations
    }
    
    /// Called after showing pixels
    /// Override in derived classes for post-show operations
    /// @param pixels the pixel controller containing LED data
    virtual void afterShow(PixelController<RGB_ORDER> &pixels) {
        // Default: no post-show operations
    }
};

/// WS2812/NeoPixel clockless controller using Adafruit_NeoPixel library as the underlying driver
/// 
/// This controller provides a simple interface for WS2812/NeoPixel LEDs using the proven
/// Adafruit_NeoPixel library for platform-specific optimizations.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam RGB_ORDER the RGB ordering for the LEDs (affects input processing, output to Adafruit is always RGB)
/// @see https://github.com/adafruit/Adafruit_NeoPixel
template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessController : public AdafruitWS2812Controller<DATA_PIN, RGB_ORDER> {
public:
    /// Constructor - creates uninitialized controller
    ClocklessController() = default;
    
    /// Destructor - automatic cleanup via base class
    ~ClocklessController() = default;

    // The base class AdafruitWS2812Controller provides all the default functionality
    // This class can be extended to add WS2812-specific customizations if needed
};

} // namespace fl

#endif  // FASTLED_USE_ADAFRUIT_NEOPIXEL
