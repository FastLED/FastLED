/// @file pixel_iterator.h
/// Non-templated low level pixel data writing class

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fl/namespace.h"
#include "rgbw.h"

#include "crgb.h"

FASTLED_NAMESPACE_BEGIN

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

  static void loadAndScale_APA102_HD(void* pixel_controller, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* brightness_out) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScale_APA102_HD(b0_out, b1_out, b2_out, brightness_out);
  }

  static void loadAndScale_WS2816_HD(void* pixel_controller, uint16_t *s0_out, uint16_t* s1_out, uint16_t* s2_out) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->loadAndScale_WS2816_HD(s0_out, s1_out, s2_out);
  }

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

  // function for getHdScale
  #if FASTLED_HD_COLOR_MIXING
  static void getHdScale(void* pixel_controller, uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
    PixelControllerT* pc = static_cast<PixelControllerT*>(pixel_controller);
    pc->getHdScale(c0, c1, c2, brightness);
  }
  #endif
};

typedef void (*loadAndScaleRGBWFunction)(void* pixel_controller, Rgbw rgbw, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* b3_out);
typedef void (*loadAndScaleRGBFunction)(void* pixel_controller, uint8_t* r_out, uint8_t* g_out, uint8_t* b_out);
typedef void (*loadAndScale_APA102_HDFunction)(void* pixel_controller, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* brightness_out);
typedef void (*loadAndScale_WS2816_HDFunction)(void* pixel_controller, uint16_t* b0_out, uint16_t* b1_out, uint16_t* b2_out);
typedef void (*stepDitheringFunction)(void* pixel_controller);
typedef void (*advanceDataFunction)(void* pixel_controller);
typedef int (*sizeFunction)(void* pixel_controller);
typedef bool (*hasFunction)(void* pixel_controller, int n);
typedef uint8_t (*globalBrightness)(void* pixel_controller);
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
      mLoadAndScale_APA102_HD = &Vtable::loadAndScale_APA102_HD;
      mLoadAndScale_WS2816_HD = &Vtable::loadAndScale_WS2816_HD;
      mStepDithering = &Vtable::stepDithering;
      mAdvanceData = &Vtable::advanceData;
      mSize = &Vtable::size;
      mHas = &Vtable::has;
      #if FASTLED_HD_COLOR_MIXING
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
    void loadAndScale_APA102_HD(uint8_t *b0_out, uint8_t *b1_out, uint8_t *b2_out, uint8_t *brightness_out) {
      mLoadAndScale_APA102_HD(mPixelController, b0_out, b1_out, b2_out, brightness_out);
    }
    void loadAndScale_WS2816_HD(uint16_t *s0_out, uint16_t *s1_out, uint16_t *s2_out) {
      mLoadAndScale_WS2816_HD(mPixelController, s0_out, s1_out, s2_out);
    }
    void stepDithering() { mStepDithering(mPixelController); }
    void advanceData() { mAdvanceData(mPixelController); }
    int size() { return mSize(mPixelController); }

    void set_rgbw(Rgbw rgbw) { mRgbw = rgbw; }
    Rgbw get_rgbw() const { return mRgbw; }

    #if FASTLED_HD_COLOR_MIXING
    void getHdScale(uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
      mGetHdScale(mPixelController, c0, c1, c2, brightness);
    }
    #endif

  private:
    // vtable emulation
    void* mPixelController = nullptr;
    Rgbw mRgbw;
    loadAndScaleRGBWFunction mLoadAndScaleRGBW = nullptr;
    loadAndScaleRGBFunction mLoadAndScaleRGB = nullptr;
    loadAndScale_APA102_HDFunction mLoadAndScale_APA102_HD = nullptr;
    loadAndScale_WS2816_HDFunction mLoadAndScale_WS2816_HD = nullptr;
    stepDitheringFunction mStepDithering = nullptr;
    advanceDataFunction mAdvanceData = nullptr;
    sizeFunction mSize = nullptr;
    hasFunction mHas = nullptr;
    #if FASTLED_HD_COLOR_MIXING
    getHdScaleFunction mGetHdScale = nullptr;
    #endif
};


FASTLED_NAMESPACE_END
