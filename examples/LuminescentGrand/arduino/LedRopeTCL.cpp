// Copyleft (c) 2012, Zach Vorhies
// Public domain, no rights reserved.
// This object holds a frame buffer and effects can be applied. This is a higher level
// object than the TCL class which this object uses for drawing.

//#include "./tcl.h"
#include <Arduino.h>
#include "../shared/color.h"
#include "../shared/framebuffer.h"
#include "../shared/settings.h"
#include "./LedRopeTCL.h"
#include "../shared/led_layout_array.h"

#include "FastLED.h"
#include "fl/dbg.h"
#include "fl/ui.h"

using namespace fl;


#define CHIPSET WS2812
#define PIN_DATA 1
#define PIN_CLOCK 2

namespace {

UIButton buttonAllWhite("All white");

ScreenMap init_screenmap() {
  LedColumns cols = LedLayoutArray();
  const int length = cols.length;
  int sum = 0;
  for (int i = 0; i < length; ++i) {
    sum += cols.array[i];
  }
  ScreenMap screen_map(sum, 0.8f);
  int curr_idx = 0;
  for (int i = 0; i < length; ++i) {
    int n = cols.array[i];
    int stagger = i % 2 ? 4 : 0;
    for (int j = 0; j < n; ++j) {
      fl::vec2f xy(i*4, j*8 + stagger);
      screen_map.set(curr_idx++, xy);
    }
  }

  return screen_map;
}

}


///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::PreDrawSetup() {
  if (!lazy_initialized_) {
    // This used to do something, now it does nothing.
    lazy_initialized_ = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawBeginDraw() {
  PreDrawSetup();
  led_buffer_.clear();
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawDrawPixel(const Color3i& c) {
  RawDrawPixel(c.r_, c.g_, c.b_);
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawDrawPixel(byte r, byte g, byte b) {
  if (led_buffer_.size() >= mScreenMap.getLength()) {
    return;
  }
  if (buttonAllWhite.isPressed()) {
    r = 0xff;
    g = 0xff;
    b = 0xff;
  }
  CRGB c(r, g, b);
  led_buffer_.push_back(CRGB(r, g, b));
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawDrawPixels(const Color3i& c, int n) {
  for (int i = 0; i < n; ++i) {
    RawDrawPixel(c);
  }
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::set_draw_offset(int val) {
  draw_offset_ = constrain(val, 0, frame_buffer_.length());
}


///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawCommitDraw() {
  FASTLED_WARN("\n\n############## COMMIT DRAW ################\n\n");
  if (!controller_added_) {
    controller_added_ = true;
    CRGB* leds = led_buffer_.data();
    size_t n_leds = led_buffer_.size();
    FastLED.addLeds<APA102, PIN_DATA, PIN_CLOCK>(leds, n_leds).setScreenMap(mScreenMap);
  }
  FASTLED_WARN("FastLED.show");
  FastLED.show();
}

///////////////////////////////////////////////////////////////////////////////
LedRopeTCL::LedRopeTCL(int n_pixels)
	: draw_offset_(0), lazy_initialized_(false), frame_buffer_(n_pixels) {
  mScreenMap = init_screenmap();
  led_buffer_.reserve(mScreenMap.getLength());
}

///////////////////////////////////////////////////////////////////////////////
LedRopeTCL::~LedRopeTCL() {
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::Draw() {
  RawBeginDraw();

  const Color3i* begin = GetIterator(0);
  const Color3i* middle = GetIterator(draw_offset_);
  const Color3i* end = GetIterator(length() - 1);

  for (const Color3i* it = middle; it != end; ++it) {
    RawDrawPixel(*it);
  }
  for (const Color3i* it = begin; it != middle; ++it) {
    RawDrawPixel(*it);
  }
  RawCommitDraw();
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::DrawSequentialRepeat(int repeat) {
  RawBeginDraw();

  const Color3i* begin = GetIterator(0);
  const Color3i* middle = GetIterator(draw_offset_);
  const Color3i* end = GetIterator(length());
  for (const Color3i* it = middle; it != end; ++it) {
    for (int i = 0; i < repeat; ++i) {
      RawDrawPixel(it->r_, it->g_, it->b_);
    }
  }
  for (const Color3i* it = begin; it != middle; ++it) {
    for (int i = 0; i < repeat; ++i) {
      RawDrawPixel(it->r_, it->g_, it->b_);
    }
  }
  RawCommitDraw();
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::DrawRepeat(const int* value_array, int array_length) {
  RawBeginDraw();
  
  // Make sure that the number of colors to repeat does not exceed the length
  // of the rope.
  const int len = MIN(array_length, frame_buffer_.length());

  for (int i = 0; i < len; ++i) {
     const Color3i* cur_color = GetIterator(i);  // Current color.
     const int repeat_count = value_array[i];
     // Repeatedly send the same color down the led rope.
     for (int k = 0; k < repeat_count; ++k) {
       RawDrawPixel(cur_color->r_, cur_color->g_, cur_color->b_);
     }
  }
  // Finish the drawing.
  RawCommitDraw();
}

