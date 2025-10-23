// @filter: FASTLED_HAS_AUDIO_INPUT and (memory is high)

// I2S Audio Example (for ESP32 as of 2025-September)
// This example demonstrates using I2S audio input to control FastLED strips
// Based on audio levels from microphone or line input

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "fl/audio_input.h"
#include "./AudioInput.h"
