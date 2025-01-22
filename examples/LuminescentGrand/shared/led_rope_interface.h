
#ifndef LED_ROPE_INTERFACE_H_
#define LED_ROPE_INTERFACE_H_

#include "./color.h"

class LedRopeInterface {
 public:
  virtual ~LedRopeInterface() {}
  virtual void Set(int i, const Color3i& c) = 0;

  virtual void Set(int i, int length, const Color3i& color) {
    for (int j = 0; j < length; ++j) {
      Set(i + j, color);
    }
  }

  virtual Color3i* GetIterator(int i)  = 0;

  virtual int length() const = 0;

  virtual void DrawSequentialRepeat(int repeat) = 0;
  virtual void DrawRepeat(const int* value_array, int array_length) = 0;

  virtual void RawBeginDraw() = 0;
  virtual void RawDrawPixel(const Color3i& c) = 0;
  virtual void RawDrawPixels(const Color3i& c, int n) = 0;
  virtual void RawDrawPixel(uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void RawCommitDraw() = 0;

};

#endif  // LED_ROPE_INTERFACE_H_
