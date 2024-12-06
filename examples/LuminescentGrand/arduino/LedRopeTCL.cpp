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

#include "FastLED.h"
#include "fl/dbg.h"


#define CHIPSET WS2812
#define PIN_LEDS 0

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::PreDrawSetup() {
  if (!lazy_initialized_) {

    lazy_initialized_ = false;
    int num_leds = frame_buffer_.length();
    Serial.print("Initializing LEDs, num_leds: "); Serial.println(num_leds);
    //tcl_.begin();
    //tcl_.sendEmptyFrame();
    // Clears upto 100,000 lights.
    //for (int i = 0; i < 100000; ++i) {
    //  RawDrawPixel(0, 0, 0);
    //}
    //tcl_.sendEmptyFrame();
    lazy_initialized_ = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawBeginDraw() {
  PreDrawSetup();
  //tcl_.sendEmptyFrame();
  led_buffer_.clear();
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawDrawPixel(const Color3i& c) {
  RawDrawPixel(c.r_, c.g_, c.b_);
}

///////////////////////////////////////////////////////////////////////////////
void LedRopeTCL::RawDrawPixel(byte r, byte g, byte b) {
  CRGB c(r, g, b);
  size_t idx = led_buffer_.size();
  // FASTLED_DBG("[" << idx << "] RawDrawPixels: " << c.toString().c_str());
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
  //tcl_.sendEmptyFrame();
  if (!controller_added_) {
    controller_added_ = true;
    CRGB* leds = led_buffer_.data();
    size_t n_leds = led_buffer_.size();
    //FastLED.addLeds<TCL, 0, 0>(0, 0);
    FastLED.addLeds<WS2812, PIN_LEDS>(leds, n_leds);
  }
  FastLED.show();
  //FASTLED_DBG("led_buffer after clear: " << led_buffer_.size());
}

///////////////////////////////////////////////////////////////////////////////
LedRopeTCL::LedRopeTCL(int n_pixels)
	: frame_buffer_(n_pixels), draw_offset_(0), lazy_initialized_(false) {
  if (kUseLedCurtin) {
    //tcl_.setNewLedChipset(true);
  }
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
  const int len = min(array_length, frame_buffer_.length());

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

