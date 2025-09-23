#pragma once

#include "FastLED.h"

// MPEG1 video data (minimal header) representing 2x2 pixel patterns:
// Frame 0: Top-left=Red, Top-right=White, Bottom-left=Blue, Bottom-right=Black
// Frame 1: Top-left=White, Top-right=Blue, Bottom-left=Black, Bottom-right=Red
// Note: This is a minimal MPEG1 sequence header for testing purposes
const uint8_t FL_PROGMEM sampleMpeg1Data[] = {
  0x00, 0x00, 0x01, 0xb3, 0x00, 0x10, 0x00, 0x10, 0x14, 0x08, 0xff, 0xff, 
  0x00, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0xb7
};
const size_t sampleMpeg1DataLength = 22;
