// Copyleft (c) 2012, Zach Vorhies
// Public domain, no rights reserved.
// This object holds a frame buffer and effects can be applied. This is a higher level
// object than the TCL class which this object uses for drawing.

#ifndef LED_REPE_TCL_H_
#define LED_REPE_TCL_H_

#include <Arduino.h>
#include "../shared/color.h"
#include "../shared/framebuffer.h"
#include "../shared/led_rope_interface.h"

#include "fl/vector.h"
#include "crgb.h"
#include "fl/screenmap.h"

// LedRopeTCL is a C++ wrapper around the Total Control Lighting LED rope
// device driver (TCL.h). This wrapper includes automatic setup of the LED
// rope and allows the user to use a graphics-state like interface for
// talking to the rope. A copy of the rope led state is held in this class
// which makes blending operations easier. After all changes by the user
// are applied to the rope, the hardware is updated via an explicit Draw()
// command.
//
// Whole-rope blink Example:
//  #include <SPI.h>
//  #include <TCL.h>          // From CoolNeon (https://bitbucket.org/devries/arduino-tcl)
//  #include "LedRopeTCL.h"
//  LedRopeTCL led_rope(100); // 100 led-strand.
//
//  void setup() {}  // No setup necessary for Led rope.
//  void loop() {
//    led_rope.FillColor(LedRopeTCL::Color3i::Black());
//    led_rope.Draw();
//    delay(1000);
//    led_rope.FillColor(LedRopeTCL::Color3i::White());
//    led_rope.Draw();
//    delay(1000);
//  }


class LedRopeTCL : public LedRopeInterface {
 public:
  LedRopeTCL(int n_pixels);
  virtual ~LedRopeTCL();

  void Draw();
  void DrawSequentialRepeat(int repeat);
  void DrawRepeat(const int* value_array, int array_length);
  void set_draw_offset(int val);
  
  virtual void Set(int i, const Color3i& c) {
    frame_buffer_.Set(i, c);
  }

  Color3i* GetIterator(int i) {
    return frame_buffer_.GetIterator(i);
  }

  int length() const { return frame_buffer_.length(); }

  void RawBeginDraw();
  void RawDrawPixel(const Color3i& c);
  void RawDrawPixels(const Color3i& c, int n);
  void RawDrawPixel(uint8_t r, uint8_t g, uint8_t b);
  void RawCommitDraw();

 protected:
  void PreDrawSetup();
  int draw_offset_ = 0;
  bool lazy_initialized_;
  FrameBuffer frame_buffer_;
  bool controller_added_ = false;
  fl::HeapVector<CRGB> led_buffer_;
  fl::ScreenMap mScreenMap;
};

#endif  // LED_REPE_TCL_H_
