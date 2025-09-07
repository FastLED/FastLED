// I2S Audio Example (for ESP32 as of 2025-September)
// This example demonstrates using I2S audio input to control FastLED strips
// Based on audio levels from microphone or line input

#include "fl/audio_input.h"

#if FASTLED_HAS_AUDIO_INPUT
#include "./AudioInput.h"
#else
#include "platforms/sketch_fake.hpp"
#endif  // FASTLED_HAS_AUDIO_INPUT
