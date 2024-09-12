
#pragma once

#include "namespace.h"
#include "rgbw.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
};

typedef void (*loadAndScaleRGBWFunction)(void* pixel_controller, Rgbw rgbw, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* b3_out);
typedef void (*loadAndScaleRGBFunction)(void* pixel_controller, uint8_t* r_out, uint8_t* g_out, uint8_t* b_out);
typedef void (*loadAndScale_APA102_HDFunction)(void* pixel_controller, uint8_t* b0_out, uint8_t* b1_out, uint8_t* b2_out, uint8_t* brightness_out);
typedef void (*stepDitheringFunction)(void* pixel_controller);
typedef void (*advanceDataFunction)(void* pixel_controller);
typedef int (*sizeFunction)(void* pixel_controller);
typedef bool (*hasFunction)(void* pixel_controller, int n);


// PixelIterator is turns a PixelController<> into a concrete object that can be used to iterate
// over pixels and transform them into driver data. See PixelController<>::as_iterator() for how
// to create a PixelIterator.
class PixelIterator {
  public:
    template<typename PixelControllerT>
    PixelIterator(PixelControllerT* pc, Rgbw rgbw) : mPixelController(pc), mRgbw(rgbw) {
      // manually build up a vtable.
      typedef PixelControllerVtable<PixelControllerT> Vtable;
      mLoadAndScaleRGBW = &Vtable::loadAndScaleRGBW;
      mLoadAndScaleRGB = &Vtable::loadAndScaleRGB;
      mLoadAndScale_APA102_HD = &Vtable::loadAndScale_APA102_HD;
      mStepDithering = &Vtable::stepDithering;
      mAdvanceData = &Vtable::advanceData;
      mSize = &Vtable::size;
      mHas = &Vtable::has;
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
    void stepDithering() { mStepDithering(mPixelController); }
    void advanceData() { mAdvanceData(mPixelController); }
    int size() { return mSize(mPixelController); }

    void set_rgbw(Rgbw rgbw) { mRgbw = rgbw; }
    Rgbw get_rgbw() const { return mRgbw; }

  private:
    // vtable emulation
    void* mPixelController;
    Rgbw mRgbw;
    loadAndScaleRGBWFunction mLoadAndScaleRGBW;
    loadAndScaleRGBFunction mLoadAndScaleRGB;
    loadAndScale_APA102_HDFunction mLoadAndScale_APA102_HD;
    stepDitheringFunction mStepDithering;
    advanceDataFunction mAdvanceData;
    sizeFunction mSize;
    hasFunction mHas;
};


FASTLED_NAMESPACE_END
