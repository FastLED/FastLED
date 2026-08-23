#pragma once

// ok no namespace fl

/// @file platforms/default_pins.h
/// @brief Default LED pins for examples and quick-start sketches.
///
/// Examples have to name *some* pin, and a literal in a sketch is a guess about
/// hardware the sketch cannot see. These macros let a board state the answer
/// instead. A board defines them next to its `_FL_DEFPIN` table -- the one place
/// that actually knows which pins exist -- and the fallbacks below apply
/// everywhere else, so no existing platform changes behaviour.
///
/// Why this exists: `Blink` hardcoded pin 3 and `Apa102` hardcoded pins 1 and 2.
/// Adafruit Feather M4 has neither pin 2 nor pin 3 (`fastpin_arm_d51.h` says so
/// outright: "no pins 2 3"), so both examples failed to compile for it with
/// `FastPin<3>::validpin()` / `FastPin<2>::validpin()` static assertions -- the
/// error text is about noisy or read-only pins, which sends you looking in
/// entirely the wrong place. Nobody noticed because SAMD had never built at all
/// until FastLED#4011.
///
/// These are compile-time defaults, not a claim about wiring. A sketch that
/// defines `PIN_DATA` (or the SPI pins) before including FastLED still wins,
/// and a board build flag still overrides both.
///
/// Included from `FastLED.h` after `platforms.h`, so board pin tables have
/// already had their say.

/// @brief Default data pin for single-wire (clockless) chipsets: WS2812, etc.
#ifndef CLOCKLESS_PIN_1
#define CLOCKLESS_PIN_1 3
#endif

/// @brief Default data pin for clocked/SPI chipsets: APA102, LPD8806, etc.
#ifndef SPI_PIN_DATA_1
#define SPI_PIN_DATA_1 1
#endif

/// @brief Default clock pin for clocked/SPI chipsets.
#ifndef SPI_PIN_CLOCK_1
#define SPI_PIN_CLOCK_1 2
#endif
