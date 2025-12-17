#pragma once

#include "fl/codec/jpeg.h"
#include "fl/codec/gif.h"
#include "fl/codec/mpeg1.h"
#include "fl/bytestreammemory.h"
#include "fl/xymap.h"
#include "fl/stl/sstream.h"
#include "fl/stl/function.h"

namespace CodecProcessor {

// Configuration constants
extern const int TARGET_FPS;  // TODO: make configurable.

// Codec processing functions
void processJpeg();
void processGif();
void processMpeg1();

// Utility functions
void processCodecWithTiming(const char* codecName, fl::function<void()> codecFunc);
void displayFrameOnLEDs(const fl::Frame& frame);
CRGB getPixelFromFrame(const fl::Frame& frame, int x, int y);
void showDecodedMessage(const char* format);

// LED array access - must be set from main sketch
extern CRGB* leds;
extern int numLeds;
extern int ledWidth;
extern int ledHeight;

} // namespace CodecProcessor
