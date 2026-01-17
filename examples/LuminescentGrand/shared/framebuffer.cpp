

#include <Arduino.h>


#include "./framebuffer.h"

#include "./color.h"
#include "fl/stl/malloc.h"

FrameBufferBase::FrameBufferBase(Color3i* array, int n_pixels)
    : mColorArray(array), mNColorArray(n_pixels) {}

FrameBufferBase::~FrameBufferBase() {}

void FrameBufferBase::Set(int i, const Color3i& c) {
  mColorArray[i] = c;
}
void FrameBufferBase::Set(int i, int length, const Color3i& color) {
  for (int j = 0; j < length; ++j) {
    Set(i + j, color);
  }
}
void FrameBufferBase::FillColor(const Color3i& color) {
  for (int i = 0; i < mNColorArray; ++i) {
    mColorArray[i] = color;
  }
}
void FrameBufferBase::ApplyBlendSubtract(const Color3i& color) {
  for (int i = 0; i < mNColorArray; ++i) {
    mColorArray[i].Sub(color);
  }
}
void FrameBufferBase::ApplyBlendAdd(const Color3i& color) {
  for (int i = 0; i < mNColorArray; ++i) {
    mColorArray[i].Add(color);
  }
}
void FrameBufferBase::ApplyBlendMultiply(const Color3i& color) {
  for (int i = 0; i < mNColorArray; ++i) {
    mColorArray[i].Mul(color);
  }
}
Color3i* FrameBufferBase::GetIterator(int i) {
  return mColorArray + i;
}
// Length in pixels.
int FrameBufferBase::length() const { return mNColorArray; }

FrameBuffer::FrameBuffer(int n_pixels)
    : FrameBufferBase(static_cast<Color3i*>(fl::malloc(sizeof(Color3i) * n_pixels)),
                        n_pixels) {
}

FrameBuffer::~FrameBuffer() {
  fl::free(mColorArray);
  mColorArray = nullptr;
  mNColorArray = 0;
}
