#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "platforms/arm/teensy/audio_input_teensy_config.h"

/// @file platforms/arm/teensy/ldf_headers.h
/// Teensy PlatformIO Library Dependency Finder (LDF) hints
///
/// FastLED's Teensy I2S implementation is private platform code.  Do not add
/// the external PJRC Audio.h umbrella here: that header causes both PlatformIO
/// LDF and fbuild to select the full Audio, SD, and SerialFlash stacks for a
/// sketch that merely includes FastLED.h.
