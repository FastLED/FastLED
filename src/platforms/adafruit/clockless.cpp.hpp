// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

// IWYU pragma: private


// ok no namespace fl
#include "fl/stl/has_include.h"

// Selecting the real Adafruit_NeoPixel driver takes an explicit opt-in, not
// just a visible header.
//
// This file is compiled into FastLED's own translation unit (via
// platforms/_build.cpp.hpp), so a `#define FASTLED_USE_ADAFRUIT_NEOPIXEL` or an
// `#include <Adafruit_NeoPixel.h>` written in the sketch is not visible here --
// only build-level defines (`-D`) are. That left `FL_HAS_INCLUDE` as the sole
// signal, and header visibility does not imply the library will link:
//
//   The Teensy Arduino core ships Adafruit_NeoPixel inside its own
//   `libraries/` folder, so the header always resolves on every Teensy board.
//   Its sources are only compiled when the dependency finder selects the
//   library, and the finder considers only unconditional includes in the
//   *sketch* (FastLED/fbuild#1214 -- a deliberate, regression-tested
//   invariant). FastLED therefore compiled the real driver against a library
//   that was never on the link line, and every teensy build failed with
//   `undefined reference to Adafruit_NeoPixel::*` (FastLED#3838).
//
// Requiring the build-level define makes the dependency explicit and matches
// what platforms/adafruit/README.md already documents this macro to mean. The
// fake driver is a safe default: it satisfies the same interface and warns at
// runtime instead of breaking the link.
#if defined(FASTLED_USE_ADAFRUIT_NEOPIXEL) && FL_HAS_INCLUDE(<Adafruit_NeoPixel.h>)
// IWYU pragma: begin_keep
#include <Adafruit_NeoPixel.h>
// IWYU pragma: end_keep
#if defined(NEO_RGBW)
#include "platforms/adafruit/clockless_real.hpp"
#else
#include "platforms/adafruit/clockless_fake.hpp"
#endif
#else
#include "platforms/adafruit/clockless_fake.hpp"
#endif
