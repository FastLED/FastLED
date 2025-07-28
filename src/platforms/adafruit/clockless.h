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

/// Generic template driver for Adafruit-like clockless controllers
/// 
/// This template provides the core functionality for any Adafruit_NeoPixel-based
/// clockless controller. It handles the common patterns of initialization,
/// pixel conversion, and output while allowing for customization through
/// template parameters and virtual methods.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam T1 timing parameter (ignored, for template compatibility)
/// @tparam T2 timing parameter (ignored, for template compatibility) 
/// @tparam T3 timing parameter (ignored, for template compatibility)
/// @tparam RGB_ORDER the RGB ordering for the LEDs (affects input processing, output to Adafruit is always RGB)
/// @tparam XTRA0 extra parameter (ignored, for template compatibility)
/// @tparam FLIP flip parameter (ignored, for template compatibility)
/// @tparam WAIT_TIME wait time parameter (ignored, for template compatibility)
template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, 
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class AdafruitLikeClocklessT : public CPixelLEDController<RGB_ORDER> {
private:
    fl::unique_ptr<Adafruit_NeoPixel> mNeoPixel;
    bool mInitialized;

public:
    /// Constructor - creates uninitialized controller
    AdafruitLikeClocklessT() : mNeoPixel(nullptr), mInitialized(false) {}
    
    /// Destructor - automatic cleanup via unique_ptr
    virtual ~AdafruitLikeClocklessT() = default;

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
/// This controller follows the standard FastLED clockless controller template signature
/// and provides duck typing compatibility with other FastLED clockless controllers.
/// The timing parameters (T1, T2, T3) are ignored since Adafruit_NeoPixel handles
/// timing internally with platform-specific optimizations.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam T1 timing parameter (ignored, for template compatibility)
/// @tparam T2 timing parameter (ignored, for template compatibility) 
/// @tparam T3 timing parameter (ignored, for template compatibility)
/// @tparam RGB_ORDER the RGB ordering for the LEDs (affects input processing, output to Adafruit is always RGB)
/// @tparam XTRA0 extra parameter (ignored, for template compatibility)
/// @tparam FLIP flip parameter (ignored, for template compatibility)
/// @tparam WAIT_TIME wait time parameter (ignored, for template compatibility)
/// @see https://github.com/adafruit/Adafruit_NeoPixel
template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, 
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public AdafruitLikeClocklessT<DATA_PIN, T1, T2, T3, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {
public:
    /// Constructor - creates uninitialized controller
    ClocklessController() = default;
    
    /// Destructor - automatic cleanup via base class
    ~ClocklessController() = default;

    // The base class AdafruitLikeClocklessT provides all the default functionality
    // This class can be extended to add WS2812-specific customizations if needed
};

} // namespace fl

#endif  // FASTLED_USE_ADAFRUIT_NEOPIXEL
