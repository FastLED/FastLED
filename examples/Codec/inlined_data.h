#pragma once

#include "FastLED.h"

// Multimedia codec sample data
// All sample data is stored as PROGMEM arrays for memory efficiency

namespace CodecData {

// JPEG sample data
extern const uint8_t FL_PROGMEM sampleJpegData[];
extern const size_t sampleJpegDataLength;

// WebP sample data
extern const uint8_t FL_PROGMEM sampleWebpData[];
extern const size_t sampleWebpDataLength;

// GIF sample data
extern const uint8_t FL_PROGMEM sampleGifData[];
extern const size_t sampleGifDataLength;

// MPEG1 sample data
extern const uint8_t FL_PROGMEM sampleMpeg1Data[];
extern const size_t sampleMpeg1DataLength;

} // namespace CodecData