/// @file fl/chipsets/encoders/pixel_iterator.h
/// Non-templated low level pixel data writing class

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/iterator.h"
#include "fl/deprecated.h"
#include "fl/gamma.h"
#include "fl/rgbw.h"
#include "crgb.h"
#include "lib8tion/intmap.h"
#include "fl/chipsets/encoders/ws2801.h"
#include "fl/chipsets/encoders/ws2803.h"
#include "fl/chipsets/encoders/ws2812.h"
#include "fl/chipsets/encoders/apa102.h"
#include "fl/chipsets/encoders/sk9822.h"
#include "fl/chipsets/encoders/hd108.h"
#include "fl/chipsets/encoders/p9813.h"
#include "fl/chipsets/encoders/lpd8806.h"
#include "fl/chipsets/encoders/lpd6803.h"
#include "fl/chipsets/encoders/sm16716.h"

// Include adapter class definitions (but not implementations yet)
// This provides the full definitions of ScaledPixelIterator* classes
// and makeScaledPixelRange* helper functions
#include "fl/chipsets/encoders/pixel_iterator_adapters.h"

namespace fl {

// Forward declaration
class PixelIterator;

// NOTE: FASTLED_PIXEL_ITERATOR_HAS_APA102_HD flag removed - HD functions moved to encoder files
// loadAndScale_APA102_HD() moved to src/fl/chipsets/encoders/apa102.h
// loadAndScale_WS2816_HD() moved to src/fl/chipsets/encoders/ws2816.h

// Due to to the template nature of the PixelController class, the only way we can make
// it a concrete polymorphic class is to manually bind the functions and make our own
// vtable. The PixelControllerVtable is cheaper than doing fl::function<>.
template<typename PixelControllerT>
struct PixelControllerVtable {
  static void loadAndScaleRGBW(void* pixel_controller, Rgbw rgbw, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* b3_out) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScaleRGBW(rgbw, b0_out, b1_out, b2_out, b3_out);
  }

  static void loadAndScaleRGB(void* pixel_controller, uint8_t* r_out, uint8_t* g_out, uint8_t* b_out) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScaleRGB(r_out, g_out, b_out);
  }

  // NOTE: loadAndScale_APA102_HD() removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
  // NOTE: loadAndScale_WS2816_HD() removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder

  static void stepDithering(void* pixel_controller) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->stepDithering();
  }

  static void advanceData(void* pixel_controller) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->advanceData();
  }

  static int size(void* pixel_controller) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    return pc->size();
  }
  static bool has(void* pixel_controller, int n) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    return pc->has(n);
  }

  // function for loadRGBScaleAndBrightness
  #if FASTLED_HD_COLOR_MIXING
  static void loadRGBScaleAndBrightness(void* pixel_controller, uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadRGBScaleAndBrightness(c0, c1, c2, brightness);
  }

  // Deprecated: for backwards compatibility
  static void getHdScale(void* pixel_controller, uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
    loadRGBScaleAndBrightness(pixel_controller, c0, c1, c2, brightness);
  }
  #endif
};

typedef void (*loadAndScaleRGBWFunction)(void* pixel_controller, Rgbw rgbw, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* b3_out);
typedef void (*loadAndScaleRGBFunction)(void* pixel_controller, uint8_t* r_out, uint8_t* g_out, uint8_t* b_out);
// NOTE: loadAndScale_APA102_HDFunction removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
// NOTE: loadAndScale_WS2816_HDFunction removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder
typedef void (*stepDitheringFunction)(void* pixel_controller);
typedef void (*advanceDataFunction)(void* pixel_controller);
typedef int (*sizeFunction)(void* pixel_controller);
typedef bool (*hasFunction)(void* pixel_controller, int n);
typedef uint8_t (*globalBrightness)(void* pixel_controller);
typedef void (*loadRGBScaleAndBrightnessFunction)(void* pixel_controller, uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness);
typedef void (*getHdScaleFunction)(void* pixel_controller, uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness);


// PixelIterator is turns a PixelController<> into a concrete object that can be used to iterate
// over pixels and transform them into driver data. See PixelController<>::as_iterator() for how
// to create a PixelIterator.
// Note: This is designed for micro-controllers with a lot of memory. DO NOT use this in the core library
// as a PixelIterator consumes a *lot* more instruction data than an instance of PixelController<RGB_ORDER>.
// This iterator is designed for code in src/platforms/**.
class PixelIterator {
  public:
    template<typename PixelControllerT>
    PixelIterator(PixelControllerT* pc, Rgbw rgbw)
         : mPixelController(pc), mRgbw(rgbw) {
      // Manually build up a vtable.
      // Wait... what? Stupid nerds trying to show off how smart they are...
      // Why not just use a virtual function?!
      //
      // Before you think this then you should know that the alternative straight
      // forward way is to have a virtual interface class that PixelController inherits from.
      // ...and that was already tried. And if you try to do this yourself
      // this then let me tell you what is going to happen...
      //
      // EVERY SINGLE PLATFORM THAT HAS A COMPILED BINARY SIZE CHECK WILL IMMEDIATELY
      // FAIL AS THE BINARY BLOWS UP BY 10-30%!!! It doesn't matter if only one PixelController
      // with a vtable is used, gcc seems not to de-virtualize the calls. And we really care
      // about binary size since FastLED needs to run on those tiny little microcontrollers like
      // the Attiny85 (and family) which are in the sub $1 range used for commercial products.
      //
      // So to satisfy these tight memory requirements we make the dynamic dispatch used in PixelIterator
      // an optional zero-cost abstraction which doesn't affect the binary size for platforms that
      // don't use it. So that's why we are using this manual construction of the vtable that is built
      // up using template magic. If your platform has lots of memory then you'll gladly trade
      // a sliver of memory for the convenience of having a concrete implementation of
      // PixelController that you can use without having to make all your driver code a template.
      //
      // Btw, this pattern in C++ is called the "type-erasure pattern". It allows non virtual
      // polymorphism by leveraging the C++ template system to ensure type safety.
      typedef PixelControllerVtable<PixelControllerT> Vtable;
      mLoadAndScaleRGBW = &Vtable::loadAndScaleRGBW;
      mLoadAndScaleRGB = &Vtable::loadAndScaleRGB;
      // NOTE: mLoadAndScale_APA102_HD removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
      // NOTE: mLoadAndScale_WS2816_HD removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder
      mStepDithering = &Vtable::stepDithering;
      mAdvanceData = &Vtable::advanceData;
      mSize = &Vtable::size;
      mHas = &Vtable::has;
      #if FASTLED_HD_COLOR_MIXING
      mLoadRGBScaleAndBrightness = &Vtable::loadRGBScaleAndBrightness;
      mGetHdScale = &Vtable::getHdScale;
      #endif
    }

    bool has(int n) { return mHas(mPixelController, n); }
    void loadAndScaleRGBW(uint8_t *b0_out, uint8_t *b1_out, uint8_t *b2_out, uint8_t *w_out) {
      mLoadAndScaleRGBW(mPixelController, mRgbw, b0_out, b1_out, b2_out, w_out);
    }
    void loadAndScaleRGB(uint8_t *r_out, uint8_t *g_out, uint8_t *b_out) {
      mLoadAndScaleRGB(mPixelController, r_out, g_out, b_out);
    }
    // NOTE: loadAndScale_APA102_HD() removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
    // NOTE: loadAndScale_WS2816_HD() removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder
    void stepDithering() { mStepDithering(mPixelController); }
    void advanceData() { mAdvanceData(mPixelController); }
    int size() { return mSize(mPixelController); }

    void set_rgbw(Rgbw rgbw) { mRgbw = rgbw; }
    Rgbw get_rgbw() const { return mRgbw; }

    #if FASTLED_HD_COLOR_MIXING
    void loadRGBScaleAndBrightness(uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
      mLoadRGBScaleAndBrightness(mPixelController, c0, c1, c2, brightness);
    }

    FL_DEPRECATED("Use loadRGBScaleAndBrightness() instead")
    void getHdScale(uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
      loadRGBScaleAndBrightness(c0, c1, c2, brightness);
    }
    #endif

    template <typename CONTAINER_UIN8_T>
    void writeWS2812(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        if (mRgbw.active()) {
            auto range = makeScaledPixelRangeRGBW(this);
            encodeWS2812_RGBW(range.first, range.second, back_ins);
        } else {
            auto range = makeScaledPixelRangeRGB(this);
            encodeWS2812_RGB(range.first, range.second, back_ins);
        }
    }

    /// @brief Encode pixels in APA102/DOTSTAR format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @param hd_gamma Enable high-definition gamma correction (per-LED brightness)
    /// @note Protocol: [Start:32b 0x00][LED:[0xE0|bri5][B][G][R]]×N[End:⌈N/32⌉×32b 0xFF]
    template <typename CONTAINER_UIN8_T>
    void writeAPA102(CONTAINER_UIN8_T* out, bool hd_gamma = false) {
        auto back_ins = fl::back_inserter(*out);

        #if FASTLED_HD_COLOR_MIXING
        if (hd_gamma) {
            // HD gamma mode: per-LED brightness
            auto pixel_range = makeScaledPixelRangeRGB(this);
            auto brightness_range = makeScaledBrightnessRange(this);
            encodeAPA102_HD(pixel_range.first, pixel_range.second,
                                      brightness_range.first, back_ins);
            return;
        }
        #endif

        #if FASTLED_USE_GLOBAL_BRIGHTNESS == 1
        // Global brightness mode: extract from first pixel
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeAPA102_AutoBrightness(pixel_range.first, pixel_range.second,
                                              back_ins);
        #else
        // Full brightness mode
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeAPA102(pixel_range.first, pixel_range.second,
                               back_ins, 31);
        #endif
    }

    /// @brief Encode pixels in SK9822 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @param hd_gamma Enable high-definition gamma correction (per-LED brightness)
    /// @note Protocol: Same as APA102 but end frame uses 0x00 instead of 0xFF
    template <typename CONTAINER_UIN8_T>
    void writeSK9822(CONTAINER_UIN8_T* out, bool hd_gamma = false) {
        auto back_ins = fl::back_inserter(*out);

        #if FASTLED_HD_COLOR_MIXING
        if (hd_gamma) {
            // HD gamma mode: per-LED brightness
            auto pixel_range = makeScaledPixelRangeRGB(this);
            auto brightness_range = makeScaledBrightnessRange(this);
            encodeSK9822_HD(pixel_range.first, pixel_range.second,
                                      brightness_range.first, back_ins);
            return;
        }
        #endif

        #if FASTLED_USE_GLOBAL_BRIGHTNESS == 1
        // Global brightness mode: extract from first pixel
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeSK9822_AutoBrightness(pixel_range.first, pixel_range.second,
                                              back_ins);
        #else
        // Full brightness mode
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeSK9822(pixel_range.first, pixel_range.second,
                               back_ins, 31);
        #endif
    }

    // ========== SPI Chipset Encoders ==========
    // Refactored to use standalone encoder functions in src/fl/chipsets/encoders/

    /// @brief Encode pixels in WS2801 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: Simple RGB bytes, no frame overhead
    /// @note Uses unified encoder: src/fl/chipsets/encoders/ws2801.h
    template <typename CONTAINER_UIN8_T>
    void writeWS2801(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeWS2801(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in WS2803 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: Identical to WS2801
    /// @note Uses unified encoder: src/fl/chipsets/encoders/ws2803.h
    template <typename CONTAINER_UIN8_T>
    void writeWS2803(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeWS2803(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in P9813 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: [Boundary:4B][LED:flag+BGR]×N[Boundary:4B]
    template <typename CONTAINER_UIN8_T>
    void writeP9813(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeP9813(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in LPD8806 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: GRB with MSB set + latch bytes
    template <typename CONTAINER_UIN8_T>
    void writeLPD8806(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeLPD8806(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in LPD6803 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: 16-bit per LED (1 bit marker + 5-5-5 RGB)
    template <typename CONTAINER_UIN8_T>
    void writeLPD6803(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeLPD6803(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in SM16716 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: RGB with start bit for each triplet
    template <typename CONTAINER_UIN8_T>
    void writeSM16716(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeSM16716(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in HD108 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: 16-bit RGB with gamma correction and brightness control
    template <typename CONTAINER_UIN8_T>
    void writeHD108(CONTAINER_UIN8_T* out) {
        auto back_ins = fl::back_inserter(*out);

        #if FASTLED_HD_COLOR_MIXING
        // HD mode: per-LED brightness
        auto pixel_range = makeScaledPixelRangeRGB(this);
        auto brightness_range = makeScaledBrightnessRange(this);
        encodeHD108_HD(pixel_range.first, pixel_range.second,
                                 brightness_range.first, back_ins);
        #else
        // Standard mode: global brightness (255 = full)
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeHD108(pixel_range.first, pixel_range.second,
                              back_ins, 255);
        #endif
    }

  private:
    // vtable emulation
    void* mPixelController = nullptr;
    Rgbw mRgbw;
    loadAndScaleRGBWFunction mLoadAndScaleRGBW = nullptr;
    loadAndScaleRGBFunction mLoadAndScaleRGB = nullptr;
    // NOTE: mLoadAndScale_APA102_HD removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
    // NOTE: mLoadAndScale_WS2816_HD removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder
    stepDitheringFunction mStepDithering = nullptr;
    advanceDataFunction mAdvanceData = nullptr;
    sizeFunction mSize = nullptr;
    hasFunction mHas = nullptr;
    #if FASTLED_HD_COLOR_MIXING
    loadRGBScaleAndBrightnessFunction mLoadRGBScaleAndBrightness = nullptr;
    getHdScaleFunction mGetHdScale = nullptr;
    #endif
};


// ===========================================================================
// Implementation of adapter advance() methods
// ===========================================================================
// These implementations are defined here (after PixelIterator is complete)
// because they require calling methods on the fully-defined PixelIterator type.

namespace detail {

// ScaledPixelIteratorRGB implementation
inline void ScaledPixelIteratorRGB::advance() {
    if (!mPixels) {
        mHasValue = false;
        return;
    }

    if (mPixels->has(1)) {
        u8 b0, b1, b2;
        mPixels->loadAndScaleRGB(&b0, &b1, &b2);
        mCurrent = array<u8, 3>{{b0, b1, b2}};  // Wire order bytes
        mPixels->stepDithering();
        mPixels->advanceData();
        mHasValue = true;
    } else {
        mHasValue = false;
    }
}

// ScaledPixelIteratorRGBW implementation
inline void ScaledPixelIteratorRGBW::advance() {
    if (!mPixels) {
        mHasValue = false;
        return;
    }

    if (mPixels->has(1)) {
        u8 b0, b1, b2, b3;
        mPixels->loadAndScaleRGBW(&b0, &b1, &b2, &b3);
        mCurrent = array<u8, 4>{{b0, b1, b2, b3}};  // Wire order bytes
        mPixels->stepDithering();
        mPixels->advanceData();
        mHasValue = true;
    } else {
        mHasValue = false;
    }
}

// ScaledPixelIteratorBrightness implementation
inline void ScaledPixelIteratorBrightness::advance() {
    if (!mPixels) {
        mHasValue = false;
        return;
    }

    if (mPixels->has(1)) {
        #if FASTLED_HD_COLOR_MIXING
        u8 r, g, b, brightness;
        mPixels->loadRGBScaleAndBrightness(&r, &g, &b, &brightness);
        mCurrent = brightness;
        #else
        // Fallback: compute brightness from max RGB component
        u8 r, g, b;
        mPixels->loadAndScaleRGB(&r, &g, &b);
        // Use sequential comparisons to avoid nested FL_MAX (helps AVR register allocation)
        u8 max_rg = (r > g) ? r : g;
        mCurrent = (max_rg > b) ? max_rg : b;
        #endif
        mPixels->stepDithering();
        mPixels->advanceData();
        mHasValue = true;
    } else {
        mHasValue = false;
    }
}

// ScaledPixelIteratorRGB16 implementation
inline void ScaledPixelIteratorRGB16::advance() {
    if (!mPixels) {
        mHasValue = false;
        return;
    }

    if (mPixels->has(1)) {
        // Get wire-ordered, color-corrected RGB bytes from PixelIterator (RGB reordering already applied)
        u8 b0, b1, b2;
        u8 brightness;

        #if FASTLED_HD_COLOR_MIXING
        // HD mode: RGB is color-corrected but NOT brightness-scaled
        mPixels->loadRGBScaleAndBrightness(&b0, &b1, &b2, &brightness);
        #else
        // Standard mode: RGB is color-corrected AND brightness-scaled (premixed)
        mPixels->loadAndScaleRGB(&b0, &b1, &b2);
        brightness = 255;  // No separate brightness scaling needed
        #endif

        // Map 8-bit → 16-bit RGB (color correction already applied)
        u16 r16 = fl::map8_to_16(b0);
        u16 g16 = fl::map8_to_16(b1);
        u16 b16 = fl::map8_to_16(b2);

        // Apply brightness scaling in HD mode (brightness not yet applied by loadRGBScaleAndBrightness)
        if (brightness != 255) {
            r16 = scale16by8(r16, brightness);
            g16 = scale16by8(g16, brightness);
            b16 = scale16by8(b16, brightness);
        }

        mCurrent = array<u16, 3>{{r16, g16, b16}};  // Wire order 16-bit channels
        mPixels->stepDithering();
        mPixels->advanceData();
        mHasValue = true;
    } else {
        mHasValue = false;
    }
}

} // namespace detail


}  // namespace fl
