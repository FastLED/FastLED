#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "platforms/arm/teensy/audio_input_teensy_config.h"

/// @file platforms/arm/teensy/ldf_headers.h
/// Teensy PlatformIO Library Dependency Finder (LDF) hints
///
/// This file exposes Teensy framework dependencies through the FastLED include
/// chain so PlatformIO's LDF and fbuild's header scanner can select them.

// Keep the Teensy Audio dependency in the normal FastLED include chain. This
// is intentional: PlatformIO LDF and fbuild both need to see Audio.h in a
// reachable header in order to select the bundled Audio implementation.
#if FASTLED_USES_TEENSY_AUDIO_INPUT && TEENSY_AUDIO_LIBRARY_AVAILABLE
// IWYU pragma: begin_keep
#include <Audio.h>
// IWYU pragma: end_keep
#endif
