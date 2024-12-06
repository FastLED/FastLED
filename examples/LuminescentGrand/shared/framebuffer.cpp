

#include <Arduino.h>


#include "./framebuffer.h"

#include "./color.h"

FrameBufferBase::FrameBufferBase(Color3i* array, int n_pixels)
    : color_array_(array), n_color_array_(n_pixels) {}

FrameBufferBase::~FrameBufferBase() {}

void FrameBufferBase::Set(int i, const Color3i& c) {
  color_array_[i] = c;
}
void FrameBufferBase::Set(int i, int length, const Color3i& color) {
  for (int j = 0; j < length; ++j) {
    Set(i + j, color);
  }
}
void FrameBufferBase::FillColor(const Color3i& color) {
  for (int i = 0; i < n_color_array_; ++i) {
    color_array_[i] = color;
  }
}
void FrameBufferBase::ApplyBlendSubtract(const Color3i& color) {
  for (int i = 0; i < n_color_array_; ++i) {
    color_array_[i].Sub(color);
  }
}
void FrameBufferBase::ApplyBlendAdd(const Color3i& color) {
  for (int i = 0; i < n_color_array_; ++i) {
    color_array_[i].Add(color);
  }
}
void FrameBufferBase::ApplyBlendMultiply(const Color3i& color) {
  for (int i = 0; i < n_color_array_; ++i) {
    color_array_[i].Mul(color);
  }
}
Color3i* FrameBufferBase::GetIterator(int i) {
  return color_array_ + i;
}
// Length in pixels.
int FrameBufferBase::length() const { return n_color_array_; }

FrameBuffer::FrameBuffer(int n_pixels)
    : FrameBufferBase(static_cast<Color3i*>(malloc(sizeof(Color3i) * n_pixels)),
                        n_pixels) {
}

FrameBuffer::~FrameBuffer() {
  free(color_array_);
  color_array_ = NULL;
  n_color_array_ = 0;
}
