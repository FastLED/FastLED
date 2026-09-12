/// @file fl/chipsets/encoders/pixel_iterator.h
/// Non-templated low level pixel data writing class

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/iterator.h"
#include "fl/stl/compiler_control.h"
#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbww.h"
#include "crgb.h"
#include "fl/math/intmap.h"
#include "fl/system/sketch_macros.h"  // IWYU pragma: keep  (FL_PLATFORM_HAS_TINY_MEMORY)
#include "fl/stl/type_traits.h"  // IWYU pragma: keep  (declval for WideLoadBinder)
#include "fl/chipsets/encoders/ws2801.h"
#include "fl/chipsets/encoders/ws2803.h"
#include "fl/chipsets/encoders/ws2812.h"
#include "fl/chipsets/encoders/tm1812.h"
#include "fl/chipsets/encoders/tm1908.h"
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
#include "fl/stl/noexcept.h"

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
  static void loadAndScaleRGBW(void* pixel_controller, const Rgbw& rgbw, u8* b0_out, u8* b1_out, u8* b2_out, u8* b3_out) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScaleRGBW(rgbw, b0_out, b1_out, b2_out, b3_out);
  }

  // 5-channel RGBWW path (issue #2558, Phase C of #2545). Same vtable
  // pattern as loadAndScaleRGBW but produces 5 output bytes (RGB + warm-W
  // + cool-W) per pixel in EOrder + EOrderWW wire order.
  static void loadAndScaleRGBWW(void* pixel_controller, Rgbww rgbww,
                                u8* b0_out, u8* b1_out, u8* b2_out,
                                u8* b3_out, u8* b4_out) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScaleRGBWW(rgbww, b0_out, b1_out, b2_out, b3_out, b4_out);
  }

  static void loadAndScaleRGB(void* pixel_controller, u8* r_out, u8* g_out, u8* b_out) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScaleRGB(r_out, g_out, b_out);
  }

#if !FL_PLATFORM_HAS_TINY_MEMORY
  // Wide load (P8, #4042), bound only for sources that actually have one.
  //
  // Measured: binding it for every source cost 750 B on an ESP32-S3 Blink
  // build -- six copies of this thunk at 125 B, one per EOrder
  // instantiation, all doing the identical widening. Only a colour-managed
  // source has a wider pixel to offer; every PixelController just widens its
  // 8-bit one, and PixelIterator can do that once for all of them.
  static void loadAndScaleRGB16(void* pixel_controller, u16* r_out, u16* g_out, u16* b_out) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScaleRGB16(r_out, g_out, b_out);
  }
#endif

  // NOTE: loadAndScale_APA102_HD() removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
  // NOTE: loadAndScale_WS2816_HD() removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder

  static void stepDithering(void* pixel_controller) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->stepDithering();
  }

  static void advanceData(void* pixel_controller) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->advanceData();
  }

  static int size(void* pixel_controller) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    return pc->size();
  }
  static bool has(void* pixel_controller, int n) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    return pc->has(n);
  }

  // function for loadRGBScaleAndBrightness
  #if FASTLED_HD_COLOR_MIXING
  static void loadRGBScaleAndBrightness(void* pixel_controller, u8* c0, u8* c1, u8* c2, u8* brightness) FL_NO_EXCEPT {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadRGBScaleAndBrightness(c0, c1, c2, brightness);
  }

  // Deprecated: for backwards compatibility
  static void getHdScale(void* pixel_controller, u8* c0, u8* c1, u8* c2, u8* brightness) FL_NO_EXCEPT {
    loadRGBScaleAndBrightness(pixel_controller, c0, c1, c2, brightness);
  }
  #endif
};

typedef void (*loadAndScaleRGBWFunction)(void* pixel_controller, const Rgbw& rgbw, u8* b0_out, u8* b1_out, u8* b2_out, u8* b3_out);
typedef void (*loadAndScaleRGBWWFunction)(void* pixel_controller, Rgbww rgbww, u8* b0_out, u8* b1_out, u8* b2_out, u8* b3_out, u8* b4_out);
typedef void (*loadAndScaleRGBFunction)(void* pixel_controller, u8* r_out, u8* g_out, u8* b_out);
#if !FL_PLATFORM_HAS_TINY_MEMORY
typedef void (*loadAndScaleRGB16Function)(void* pixel_controller, u16* r_out, u16* g_out, u16* b_out);

/// Binds the wide thunk only when `T` declares `loadAndScaleRGB16`.
///
/// A plain `PixelController` does not, so nothing is emitted for it and the
/// pointer stays null; `PixelIterator::loadAndScaleRGB16` then widens the
/// 8-bit load in one place instead of once per colour order.
template <typename T, typename = void>
struct WideLoadBinder {
    static loadAndScaleRGB16Function get() FL_NO_EXCEPT { return nullptr; }
};

template <typename T>
struct WideLoadBinder<T, decltype(static_cast<void>(
                             fl::declval<T&>().loadAndScaleRGB16(
                                 static_cast<u16*>(nullptr),
                                 static_cast<u16*>(nullptr),
                                 static_cast<u16*>(nullptr))))> {
    static loadAndScaleRGB16Function get() FL_NO_EXCEPT {
        return &PixelControllerVtable<T>::loadAndScaleRGB16;
    }
};
#endif
// NOTE: loadAndScale_APA102_HDFunction removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
// NOTE: loadAndScale_WS2816_HDFunction removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder
typedef void (*stepDitheringFunction)(void* pixel_controller);
typedef void (*advanceDataFunction)(void* pixel_controller);
typedef int (*sizeFunction)(void* pixel_controller);
typedef bool (*hasFunction)(void* pixel_controller, int n);
typedef u8 (*globalBrightness)(void* pixel_controller);
typedef void (*loadRGBScaleAndBrightnessFunction)(void* pixel_controller, u8* c0, u8* c1, u8* c2, u8* brightness);
typedef void (*getHdScaleFunction)(void* pixel_controller, u8* c0, u8* c1, u8* c2, u8* brightness);


// PixelIterator is turns a PixelController<> into a concrete object that can be used to iterate
// over pixels and transform them into driver data. See PixelController<>::as_iterator() for how
// to create a PixelIterator.
// Note: This is designed for micro-controllers with a lot of memory. DO NOT use this in the core library
// as a PixelIterator consumes a *lot* more instruction data than an instance of PixelController<RGB_ORDER>.
// This iterator is designed for code in src/platforms/**.
class PixelIterator {
  public:
    template<typename PixelControllerT>
    PixelIterator(PixelControllerT* pc, const Rgbw& rgbw,
                  Rgbww rgbww = RgbwwInvalid::value()) FL_NO_EXCEPT
         : mPixelController(pc), mRgbw(rgbw), mRgbww(rgbww) {
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
      mLoadAndScaleRGBWW = &Vtable::loadAndScaleRGBWW;
      mLoadAndScaleRGB = &Vtable::loadAndScaleRGB;
#if !FL_PLATFORM_HAS_TINY_MEMORY
      mLoadAndScaleRGB16 = WideLoadBinder<PixelControllerT>::get();
#endif
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

    bool has(int n) FL_NO_EXCEPT { return mHas(mPixelController, n); }
    void loadAndScaleRGBW(u8 *b0_out, u8 *b1_out, u8 *b2_out, u8 *w_out) FL_NO_EXCEPT {
      mLoadAndScaleRGBW(mPixelController, mRgbw, b0_out, b1_out, b2_out, w_out);
    }
    void loadAndScaleRGBWW(u8 *b0_out, u8 *b1_out, u8 *b2_out,
                           u8 *b3_out, u8 *b4_out) FL_NO_EXCEPT {
      mLoadAndScaleRGBWW(mPixelController, mRgbww, b0_out, b1_out, b2_out, b3_out, b4_out);
    }
    void loadAndScaleRGB(u8 *r_out, u8 *g_out, u8 *b_out) FL_NO_EXCEPT {
      mLoadAndScaleRGB(mPixelController, r_out, g_out, b_out);
    }
#if !FL_PLATFORM_HAS_TINY_MEMORY
    /// One pixel at the source's own precision, wire-ordered.
    ///
    /// For an ordinary `PixelController` that is its 8-bit pixel widened
    /// exactly; for a colour-managed source it is the s16.16 device drive
    /// quantized once, to 16 bits rather than to 8 and back up.
    void loadAndScaleRGB16(u16 *r_out, u16 *g_out, u16 *b_out) FL_NO_EXCEPT {
      if (mLoadAndScaleRGB16 != nullptr) {
        mLoadAndScaleRGB16(mPixelController, r_out, g_out, b_out);
        return;
      }
      // The source has no wider pixel than its 8-bit one. Widening exactly,
      // here rather than in a per-source thunk.
      u8 r8, g8, b8;
      loadAndScaleRGB(&r8, &g8, &b8);
      *r_out = fl::map8_to_16(r8);
      *g_out = fl::map8_to_16(g8);
      *b_out = fl::map8_to_16(b8);
    }
#endif
    // NOTE: loadAndScale_APA102_HD() removed - use fl::loadAndScale_APA102_HD<RGB_ORDER>() from apa102.h encoder
    // NOTE: loadAndScale_WS2816_HD() removed - use fl::loadAndScale_WS2816_HD<RGB_ORDER>() from ws2816.h encoder
    void stepDithering() FL_NO_EXCEPT { mStepDithering(mPixelController); }
    void advanceData() FL_NO_EXCEPT { mAdvanceData(mPixelController); }
    int size() FL_NO_EXCEPT { return mSize(mPixelController); }

    void set_rgbw(const Rgbw& rgbw) FL_NO_EXCEPT { mRgbw = rgbw; }
    Rgbw get_rgbw() const FL_NO_EXCEPT { return mRgbw; }

    void set_rgbww(Rgbww rgbww) FL_NO_EXCEPT { mRgbww = rgbww; }
    Rgbww get_rgbww() const FL_NO_EXCEPT { return mRgbww; }

    #if FASTLED_HD_COLOR_MIXING
    void loadRGBScaleAndBrightness(u8* c0, u8* c1, u8* c2, u8* brightness) FL_NO_EXCEPT {
      mLoadRGBScaleAndBrightness(mPixelController, c0, c1, c2, brightness);
    }

    FL_DEPRECATED("Use loadRGBScaleAndBrightness() instead") FL_NO_EXCEPT
    void getHdScale(u8* c0, u8* c1, u8* c2, u8* brightness) {
      loadRGBScaleAndBrightness(c0, c1, c2, brightness);
    }
    #endif

    template <typename CONTAINER_UIN8_T>
    void writeWS2812(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        // (#2558) Dispatch order: RGBWW > RGBW > RGB. The variant migration
        // makes these mutually exclusive — at most one alternative is active.
        if (mRgbww.active()) {
            auto range = makeScaledPixelRangeRGBWW(this);
            encodeWS2812_RGBWW(range.first, range.second, back_ins);
        } else if (mRgbw.active()) {
            auto range = makeScaledPixelRangeRGBW(this);
            encodeWS2812_RGBW(range.first, range.second, back_ins);
        } else {
            auto range = makeScaledPixelRangeRGB(this);
            encodeWS2812_RGB(range.first, range.second, back_ins);
        }
    }

    /// @brief Encode RGBWW pixels into 12-channel TM1812 IC frames.
    template <typename CONTAINER_UIN8_T>
    void writeTM1812RGBWW(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto range = makeScaledPixelRangeRGBWW(this);
        encodeTM1812_RGBWW(range.first, range.second, back_ins);
    }

    /// @brief Encode RGB pixels with the required TM1908 command prefix.
    template <typename CONTAINER_UIN8_T>
    void writeTM1908(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto range = makeScaledPixelRangeRGB(this);
        encodeTM1908(range.first, range.second, back_ins);
    }

    /// @brief Encode pixels in APA102/DOTSTAR format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @param hd_gamma Enable high-definition gamma correction (per-LED brightness)
    /// @note Protocol: [Start:32b 0x00][LED:[0xE0|bri5][B][G][R]] x N, then [End: (N/32)+1 x 32b 0xFF]
    template <typename CONTAINER_UIN8_T>
    void writeAPA102(CONTAINER_UIN8_T* out, bool hd_gamma = false) FL_NO_EXCEPT {
        FL_UNUSED(hd_gamma);  // only read under FASTLED_HD_COLOR_MIXING
        auto back_ins = fl::back_inserter(*out);

        #if FASTLED_HD_COLOR_MIXING
        if (hd_gamma) {
            // HD gamma mode: per-LED brightness
            auto pixel_range = makeScaledPixelRangeRGB(this);
            auto brightness = makeScaledBrightness(this);
            encodeAPA102_HD(pixel_range.first, pixel_range.second,
                                      brightness, back_ins);
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
    /// @note Protocol: Same as APA102, including the all-ones end clock frame.
    template <typename CONTAINER_UIN8_T>
    void writeSK9822(CONTAINER_UIN8_T* out, bool hd_gamma = false) FL_NO_EXCEPT {
        FL_UNUSED(hd_gamma);  // only read under FASTLED_HD_COLOR_MIXING
        auto back_ins = fl::back_inserter(*out);

        #if FASTLED_HD_COLOR_MIXING
        if (hd_gamma) {
            // HD gamma mode: per-LED brightness
            auto pixel_range = makeScaledPixelRangeRGB(this);
            auto brightness = makeScaledBrightness(this);
            encodeSK9822_HD(pixel_range.first, pixel_range.second,
                                      brightness, back_ins);
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
    void writeWS2801(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeWS2801(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in WS2803 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: Identical to WS2801
    /// @note Uses unified encoder: src/fl/chipsets/encoders/ws2803.h
    template <typename CONTAINER_UIN8_T>
    void writeWS2803(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeWS2803(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in P9813 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: [Boundary:4B][LED:flag+BGR]×N[Boundary:4B]
    template <typename CONTAINER_UIN8_T>
    void writeP9813(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeP9813(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in LPD8806 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: GRB with MSB set + latch bytes
    template <typename CONTAINER_UIN8_T>
    void writeLPD8806(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeLPD8806(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in LPD6803 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: 16-bit per LED (1 bit marker + 5-5-5 RGB)
    template <typename CONTAINER_UIN8_T>
    void writeLPD6803(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeLPD6803(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in SM16716 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: RGB with start bit for each triplet
    template <typename CONTAINER_UIN8_T>
    void writeSM16716(CONTAINER_UIN8_T* out) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);
        auto pixel_range = makeScaledPixelRangeRGB(this);
        encodeSM16716(pixel_range.first, pixel_range.second, back_ins);
    }

    /// @brief Encode pixels in HD108 format (zero allocation)
    /// @param out Output buffer to write encoded bytes
    /// @note Protocol: 16-bit RGB with gamma correction and brightness control
    /// @param managed True when the source is the colour-managed one, i.e.
    ///        `loadAndScaleRGB16` yields a device drive rather than a widened
    ///        8-bit pixel. Then the fixed 2.8 gamma is skipped: it would be a
    ///        second shaping stage after the device solve (#4326).
    template <typename CONTAINER_UIN8_T>
    void writeHD108(CONTAINER_UIN8_T* out, bool managed = false) FL_NO_EXCEPT {
        auto back_ins = fl::back_inserter(*out);

#if !FL_PLATFORM_HAS_TINY_MEMORY
        if (managed) {
            // Inline rather than an `encodeHD108_wide` in hd108.h: that header
            // is included *by* this one, before `PixelIterator` exists, and a
            // concrete `PixelIterator&` parameter is not a dependent type --
            // so its body would be checked there and fail. Same reason the
            // UCS7604 wide path had to be guarded for TINY (#4326).
            //
            // `hd108GammaCorrect` is skipped by construction. It exists to
            // widen an 8-bit pixel to the 16 bits this chipset carries; the
            // managed source has already quantized its device drive once, to
            // 16 bits, so a fixed 2.8 curve on top is the second shaping stage
            // B1 and section 6 of the spec forbid after the device solve. And
            // unlike UCS7604's `mGamma.value_or(2.8f)`, this one is hardcoded
            // -- no caller could have chosen otherwise.
            for (int i = 0; i < 8; i++) {
                *back_ins++ = 0x00;  // start frame
            }
            // `hd108BrightnessHeader` discards its argument and pins every
            // gain at 31, so this is 0xFF 0xFF whatever is passed. The strip
            // brightness used to be fetched through `makeScaledBrightness`
            // purely to hand it over here, which built an adapter and called
            // its out-of-line `load()` to produce a number that reached
            // nothing. FastLED#4402.
            u8 f0, f1;
            hd108BrightnessHeader(0, &f0, &f1);
            fl::size num_leds = 0;
            while (has(1)) {
                u16 r16, g16, b16;
                loadAndScaleRGB16(&r16, &g16, &b16);
                *back_ins++ = f0;
                *back_ins++ = f1;
                *back_ins++ = static_cast<u8>(r16 >> 8);
                *back_ins++ = static_cast<u8>(r16 & 0xFF);
                *back_ins++ = static_cast<u8>(g16 >> 8);
                *back_ins++ = static_cast<u8>(g16 & 0xFF);
                *back_ins++ = static_cast<u8>(b16 >> 8);
                *back_ins++ = static_cast<u8>(b16 & 0xFF);
                stepDithering();
                advanceData();
                ++num_leds;
            }
            const fl::size latch = num_leds / 2 + 4;
            for (fl::size i = 0; i < latch; i++) {
                *back_ins++ = 0xFF;  // end frame
            }
            return;
        }
#else
        FL_UNUSED(managed);
#endif

        #if FASTLED_HD_COLOR_MIXING
        // HD mode: per-LED brightness
        auto pixel_range = makeScaledPixelRangeRGB(this);
        auto brightness = makeScaledBrightness(this);
        encodeHD108_HD(pixel_range.first, pixel_range.second,
                                 brightness, back_ins);
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
    Rgbww mRgbww;
    loadAndScaleRGBWFunction mLoadAndScaleRGBW = nullptr;
    loadAndScaleRGBWWFunction mLoadAndScaleRGBWW = nullptr;
    loadAndScaleRGBFunction mLoadAndScaleRGB = nullptr;
#if !FL_PLATFORM_HAS_TINY_MEMORY
    loadAndScaleRGB16Function mLoadAndScaleRGB16 = nullptr;
#endif
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
inline void ScaledPixelIteratorRGB::advance() FL_NO_EXCEPT {
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
inline void ScaledPixelIteratorRGBW::advance() FL_NO_EXCEPT {
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

// ScaledPixelIteratorRGBWW implementation (issue #2558)
inline void ScaledPixelIteratorRGBWW::advance() FL_NO_EXCEPT {
    if (!mPixels) {
        mHasValue = false;
        return;
    }

    if (mPixels->has(1)) {
        u8 b0, b1, b2, b3, b4;
        mPixels->loadAndScaleRGBWW(&b0, &b1, &b2, &b3, &b4);
        mCurrent = array<u8, 5>{{b0, b1, b2, b3, b4}};  // Wire-order 5 bytes
        mPixels->stepDithering();
        mPixels->advanceData();
        mHasValue = true;
    } else {
        mHasValue = false;
    }
}

#if FASTLED_HD_COLOR_MIXING
// ScaledPixelIteratorBrightness implementation
//
// Named load() rather than advance() because it advances nothing. It must not
// call stepDithering()/advanceData(): the RGB adapter alongside owns the
// shared cursor, and both moving it consumed two source pixels per emitted
// LED and halved the strip (#4321).
inline void ScaledPixelIteratorBrightness::load() FL_NO_EXCEPT {
    if (!mPixels) {
        return;
    }

    // ColorAdjustment::brightness -- a per-strip constant, so the same value
    // is correct at every position and no cursor movement is needed.
    u8 r, g, b, brightness;
    mPixels->loadRGBScaleAndBrightness(&r, &g, &b, &brightness);
    mCurrent = brightness;
}
#endif  // FASTLED_HD_COLOR_MIXING

// ScaledPixelIteratorRGB16 implementation
inline void ScaledPixelIteratorRGB16::advance() FL_NO_EXCEPT {
    if (!mPixels) {
        mHasValue = false;
        return;
    }

    if (mPixels->has(1)) {
        // Wire-ordered, colour-corrected, brightness-scaled pixel bytes.
        //
        // This used to branch on FASTLED_HD_COLOR_MIXING and call
        // loadRGBScaleAndBrightness() here, treating its three outputs as the
        // pixel. They are not the pixel: that call returns
        // ColorAdjustment::color -- the correction *scale* at full
        // brightness, a per-strip constant -- for a caller that will apply it
        // to a pixel itself. So every LED came out the same colour and the
        // frame never reached the wire (#4323). WS2816, the only user of this
        // range, showed one flat tint on every non-AVR build.
        //
        // loadAndScaleRGB() is the call that loads the pixel. It applies
        // `premixed`, which already carries brightness, so there is no
        // separate brightness step left to do. The old branch existed to
        // defer brightness to 16-bit precision, but it was producing a
        // constant rather than extra precision, so nothing is given up by
        // dropping it. Doing that properly needs a primitive that loads the
        // pixel scaled at full brightness, which PixelIterator does not
        // expose today.
#if !FL_PLATFORM_HAS_TINY_MEMORY
        // The wide load the comment above asks for (P8, #4042). For an
        // ordinary controller it is `map8_to_16` of the same 8-bit pixel, so
        // an unbound channel is byte-identical to what this did before. A
        // colour-managed source instead quantizes its s16.16 drive straight
        // to 16 bits, which is what lets a 16-bit encoder reach more than 256
        // levels.
        u16 r16, g16, b16;
        mPixels->loadAndScaleRGB16(&r16, &g16, &b16);
#else
        // TINY carries no colour pipeline, so there is nothing wider than the
        // 8-bit pixel to load and the extra function pointer is not spent.
        u8 b0, b1, b2;
        mPixels->loadAndScaleRGB(&b0, &b1, &b2);

        const u16 r16 = fl::map8_to_16(b0);
        const u16 g16 = fl::map8_to_16(b1);
        const u16 b16 = fl::map8_to_16(b2);
#endif

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
