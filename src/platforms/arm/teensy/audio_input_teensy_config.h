#pragma once

// IWYU pragma: private

// ok no namespace fl — preprocessor-only config header (no symbols, no types)

#include "fl/stl/has_include.h"
#include "platforms/arm/teensy/is_teensy.h"

// Teensy LC and Teensy 3.0 do not have enough RAM for the PJRC Audio
// library's static DMA buffers in normal FastLED example builds.
#if !defined(FASTLED_USES_TEENSY_AUDIO_INPUT)
  #if defined(FL_IS_TEENSY) && !defined(FL_IS_TEENSY_LC) &&                  \
      !defined(FL_IS_TEENSY_30) && FL_HAS_INCLUDE(<Audio.h>)
    #define FASTLED_USES_TEENSY_AUDIO_INPUT 1
  #else
    #define FASTLED_USES_TEENSY_AUDIO_INPUT 0
  #endif
#endif

#if !defined(TEENSY_AUDIO_LIBRARY_AVAILABLE)
  #if FASTLED_USES_TEENSY_AUDIO_INPUT && defined(FL_IS_TEENSY) &&            \
      FL_HAS_INCLUDE(<Audio.h>)
    #define TEENSY_AUDIO_LIBRARY_AVAILABLE 1
  #else
    #define TEENSY_AUDIO_LIBRARY_AVAILABLE 0
  #endif
#endif
