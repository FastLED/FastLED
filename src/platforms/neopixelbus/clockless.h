#pragma once

/// @file clockless.h
/// NeoPixelBus-based clockless controller implementation
/// 
/// This header provides a FastLED-compatible clockless controller that uses
/// the NeoPixelBus library as the underlying driver. This allows users
/// to leverage the extensive platform support and advanced features
/// of the NeoPixelBus library within the FastLED ecosystem.
///
/// Requirements:
/// - NeoPixelBus library must be included before FastLED
/// - The controller is only available when NeoPixelBus.h is detected

#include "fl/has_include.h"

#ifndef FASTLED_USE_NEOPIXEL_BUS
#ifdef FASTLED_DOXYGEN
#define FASTLED_USE_NEOPIXEL_BUS 1
#else
#define FASTLED_USE_NEOPIXEL_BUS 0
#endif
#endif

#if FASTLED_USE_NEOPIXEL_BUS && !FL_HAS_INCLUDE("NeoPixelBus.h")
#error "NeoPixelBus.h not found, disabling FASTLED_USE_NEOPIXEL_BUS"
#define FASTLED_USE_NEOPIXEL_BUS 0
#endif

#if FASTLED_USE_NEOPIXEL_BUS

#include "NeoPixelBus.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "controller.h"
#include "pixel_controller.h"
#include "color.h"
#include "fastled_config.h"

namespace fl {

// Forward declarations for method selection
template<int DATA_PIN>
struct NeoPixelBusMethodSelector {
#if defined(ESP32)
    using DefaultMethod = NeoEsp32Rmt0800KbpsMethod;
#elif defined(ESP8266)  
    using DefaultMethod = NeoEsp8266Uart1800KbpsMethod;
#elif defined(__AVR__)
    using DefaultMethod = NeoAvr800KbpsMethod;
#elif defined(__arm__)
    using DefaultMethod = NeoArm800KbpsMethod;
#else
    using DefaultMethod = NeoBitBangMethod;
#endif
};

// Color feature mapping based on RGB order
template<EOrder RGB_ORDER>
struct NeoPixelBusColorFeature {
    using Type = NeoGrbFeature;  // Default
};

template<>
struct NeoPixelBusColorFeature<RGB> {
    using Type = NeoRgbFeature;
};

template<>
struct NeoPixelBusColorFeature<GRB> {
    using Type = NeoGrbFeature;
};

template<>
struct NeoPixelBusColorFeature<BGR> {
    using Type = NeoBgrFeature;
};

template<>
struct NeoPixelBusColorFeature<BRG> {
    using Type = NeoBrgFeature;
};

template<>
struct NeoPixelBusColorFeature<RBG> {
    using Type = NeoRbgFeature;
};

template<>
struct NeoPixelBusColorFeature<GBR> {
    using Type = NeoGbrFeature;
};

/// Generic template driver for NeoPixelBus-like clockless controllers
/// 
/// This template provides the core functionality for any NeoPixelBus-based
/// clockless controller. It handles the common patterns of initialization,
/// pixel conversion, and output while allowing for customization through
/// template parameters and virtual methods.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam T1 timing parameter (ignored, for template compatibility)
/// @tparam T2 timing parameter (ignored, for template compatibility) 
/// @tparam T3 timing parameter (ignored, for template compatibility)
/// @tparam RGB_ORDER the RGB ordering for the LEDs (maps to NeoPixelBus color features)
/// @tparam XTRA0 extra parameter (ignored, for template compatibility)
/// @tparam FLIP flip parameter (ignored, for template compatibility)
/// @tparam WAIT_TIME wait time parameter (ignored, for template compatibility)
template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, 
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class NeoPixelBusLikeClocklessT : public CPixelLEDController<RGB_ORDER> {
public:
    using ColorFeature = typename NeoPixelBusColorFeature<RGB_ORDER>::Type;
    using Method = typename NeoPixelBusMethodSelector<DATA_PIN>::DefaultMethod;
    using BusType = NeoPixelBus<ColorFeature, Method>;

private:
    fl::unique_ptr<BusType> mPixelBus;
    bool mInitialized;

public:
    /// Constructor - creates uninitialized controller
    NeoPixelBusLikeClocklessT() : mPixelBus(nullptr), mInitialized(false) {}
    
    /// Destructor - automatic cleanup via unique_ptr
    virtual ~NeoPixelBusLikeClocklessT() = default;

    /// Initialize the controller
    /// Creates the NeoPixelBus instance with appropriate color feature and method
    virtual void init() override {
        if (!mInitialized) {
            try {
                // Create NeoPixelBus instance
                // Note: numPixels will be set when FastLED calls setLeds()
                mPixelBus = createPixelBus();
                if (!mPixelBus) {
                    FL_WARN("Failed to create NeoPixelBus instance");
                    return;
                }
                
                mPixelBus->Begin();
                mInitialized = true;
                
                // Allow derived classes to perform additional initialization
                onInitialized();
            } catch (...) {
                FL_WARN("NeoPixelBus initialization failed");
                mInitialized = false;
            }
        }
    }

    /// Output pixels to the LED strip
    /// Converts FastLED pixel data to NeoPixelBus format and displays
    /// @param pixels the pixel controller containing LED data
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        if (!mPixelBus || !mInitialized) {
            return;
        }
        
        // Update strip length if needed
        if (mPixelBus->PixelCount() != pixels.size()) {
            // Recreate bus with new pixel count
            mPixelBus.reset();
            mPixelBus = createPixelBus(pixels.size());
            if (!mPixelBus) {
                FL_WARN("Failed to recreate NeoPixelBus with new size");
                return;
            }
            mPixelBus->Begin();
        }
        
        // Allow derived classes to perform pre-show operations
        beforeShow(pixels);
        
        // Convert FastLED pixel data to NeoPixelBus format
        convertAndSetPixels(pixels);
        
        // Allow derived classes to perform post-conversion operations
        afterConversion(pixels);
        
        // Check if we can show (not busy)
        if (mPixelBus->CanShow()) {
            // Output to LEDs via NeoPixelBus library
            mPixelBus->Show();
        }
        
        // Allow derived classes to perform post-show operations
        afterShow(pixels);
    }

protected:
    /// Get the NeoPixelBus instance (for derived classes)
    BusType* getPixelBus() const {
        return mPixelBus.get();
    }
    
    /// Check if the controller is initialized
    bool isInitialized() const {
        return mInitialized;
    }
    
    /// Create the NeoPixelBus instance
    /// Override this in derived classes to customize bus creation
    /// @param pixelCount number of pixels (0 for default)
    /// @return unique pointer to the created bus
    virtual fl::unique_ptr<BusType> createPixelBus(uint16_t pixelCount = 0) {
        return fl::make_unique<BusType>(pixelCount, DATA_PIN);
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
    
    /// Convert and set pixels in the NeoPixelBus instance
    /// Override in derived classes to customize pixel conversion
    /// @param pixels the pixel controller containing LED data
    virtual void convertAndSetPixels(PixelController<RGB_ORDER> &pixels) {
        auto iterator = pixels.as_iterator(RgbwInvalid());
        for (int i = 0; iterator.has(1); ++i) {
            fl::u8 r, g, b;
            iterator.loadAndScaleRGB(&r, &g, &b);
            
            // Convert to NeoPixelBus color type
            RgbColor color(r, g, b);
            mPixelBus->SetPixelColor(i, color);
            
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

/// WS2812/NeoPixel clockless controller using NeoPixelBus library as the underlying driver
/// 
/// This controller follows the standard FastLED clockless controller template signature
/// and provides duck typing compatibility with other FastLED clockless controllers.
/// The timing parameters (T1, T2, T3) are ignored since NeoPixelBus handles
/// timing internally with platform-specific optimizations.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam T1 timing parameter (ignored, for template compatibility)
/// @tparam T2 timing parameter (ignored, for template compatibility) 
/// @tparam T3 timing parameter (ignored, for template compatibility)
/// @tparam RGB_ORDER the RGB ordering for the LEDs (maps to NeoPixelBus color features)
/// @tparam XTRA0 extra parameter (ignored, for template compatibility)
/// @tparam FLIP flip parameter (ignored, for template compatibility)
/// @tparam WAIT_TIME wait time parameter (ignored, for template compatibility)
/// @see https://github.com/Makuna/NeoPixelBus
template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, 
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public NeoPixelBusLikeClocklessT<DATA_PIN, T1, T2, T3, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {
public:
    /// Constructor - creates uninitialized controller
    ClocklessController() = default;
    
    /// Destructor - automatic cleanup via base class
    ~ClocklessController() = default;

    // The base class NeoPixelBusLikeClocklessT provides all the default functionality
    // This class can be extended to add WS2812-specific customizations if needed
};

/// RGBW variant controller using NeoPixelBus with white channel support
/// 
/// This controller provides RGBW support with proper white channel extraction.
/// It automatically extracts white from RGB components to optimize color output.
///
/// @tparam DATA_PIN the data pin for the LED strip
/// @tparam T1 timing parameter (ignored, for template compatibility)
/// @tparam T2 timing parameter (ignored, for template compatibility) 
/// @tparam T3 timing parameter (ignored, for template compatibility)
/// @tparam RGB_ORDER the RGB ordering for the LEDs
template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, 
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class NeoPixelBusRGBWController : public CPixelLEDController<RGB_ORDER> {
public:
    using Method = typename NeoPixelBusMethodSelector<DATA_PIN>::DefaultMethod;
    using BusType = NeoPixelBus<NeoGrbwFeature, Method>;  // Always use RGBW feature

private:
    fl::unique_ptr<BusType> mPixelBus;
    bool mInitialized;

public:
    /// Constructor - creates uninitialized controller
    NeoPixelBusRGBWController() : mPixelBus(nullptr), mInitialized(false) {}
    
    /// Destructor - automatic cleanup via unique_ptr
    virtual ~NeoPixelBusRGBWController() = default;

    /// Initialize the controller
    virtual void init() override {
        if (!mInitialized) {
            try {
                mPixelBus = fl::make_unique<BusType>(0, DATA_PIN);
                if (!mPixelBus) {
                    FL_WARN("Failed to create RGBW NeoPixelBus instance");
                    return;
                }
                
                mPixelBus->Begin();
                mInitialized = true;
                
                onInitialized();
            } catch (...) {
                FL_WARN("RGBW NeoPixelBus initialization failed");
                mInitialized = false;
            }
        }
    }

    /// Output pixels to the LED strip with RGBW conversion
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        if (!mPixelBus || !mInitialized) {
            return;
        }
        
        // Update strip length if needed
        if (mPixelBus->PixelCount() != pixels.size()) {
            mPixelBus.reset();
            mPixelBus = fl::make_unique<BusType>(pixels.size(), DATA_PIN);
            if (!mPixelBus) {
                FL_WARN("Failed to recreate RGBW NeoPixelBus with new size");
                return;
            }
            mPixelBus->Begin();
        }
        
        beforeShow(pixels);
        convertAndSetPixels(pixels);
        afterConversion(pixels);
        
        if (mPixelBus->CanShow()) {
            mPixelBus->Show();
        }
        
        afterShow(pixels);
    }

protected:
    /// Get the NeoPixelBus instance (for derived classes)
    BusType* getPixelBus() const {
        return mPixelBus.get();
    }
    
    /// Check if the controller is initialized
    bool isInitialized() const {
        return mInitialized;
    }
    
    /// Called after successful initialization
    virtual void onInitialized() {
        // Default: no additional initialization
    }
    
    /// Called before showing pixels
    virtual void beforeShow(PixelController<RGB_ORDER> &pixels) {
        // Default: no pre-show operations
    }
    
    /// Convert RGB to RGBW with white channel extraction
    virtual void convertAndSetPixels(PixelController<RGB_ORDER> &pixels) {
        auto iterator = pixels.as_iterator(RgbwInvalid());
        for (int i = 0; iterator.has(1); ++i) {
            fl::u8 r, g, b;
            iterator.loadAndScaleRGB(&r, &g, &b);
            
            // Extract white component (simple algorithm - minimum of RGB)
            fl::u8 white = fl::min(r, fl::min(g, b));
            r -= white; 
            g -= white; 
            b -= white;
            
            // Convert to NeoPixelBus RGBW color type
            RgbwColor color(r, g, b, white);
            mPixelBus->SetPixelColor(i, color);
            
            iterator.advanceData();
        }
    }
    
    /// Called after pixel conversion but before show()
    virtual void afterConversion(PixelController<RGB_ORDER> &pixels) {
        // Default: no post-conversion operations
    }
    
    /// Called after showing pixels
    virtual void afterShow(PixelController<RGB_ORDER> &pixels) {
        // Default: no post-show operations
    }
};

} // namespace fl

#endif  // FASTLED_USE_NEOPIXEL_BUS 
