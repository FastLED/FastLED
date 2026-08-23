#pragma once

// ok no namespace fl

/// @file platforms/default_pins.h
/// @brief Default LED pins for examples and quick-start sketches.
///
/// The intent is a one-pin setup that works on every platform FastLED
/// supports: an example says `FL_PIN_CLOCKLESS_1` and gets a pin that is
/// actually usable for a single-wire strip on the board being built for, with
/// no edit to the sketch.
///
/// A board declares its value beside its `_FL_DEFPIN` table -- the one place
/// that knows which pins exist. Examples have to name *some* pin, and a literal
/// in a sketch is a guess about hardware the sketch cannot see.
///
/// Status: only the SAMD boards that needed one declare a value so far. The
/// fallbacks below are the literals `Blink` and `Apa102` already hardcoded, so
/// adding this indirection changed no platform's behaviour -- but equal to the
/// old literal is not the same as correct. FastLED#4018 tracks auditing and
/// declaring a real value per platform, including the part `validpin()` cannot
/// check: a pin can exist and still be a bad default if it is a strapping pin,
/// the UART, or already committed on common dev boards.
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
#ifndef FL_PIN_CLOCKLESS_1
#define FL_PIN_CLOCKLESS_1 3
#endif

/// @brief Default data pin for clocked/SPI chipsets: APA102, LPD8806, etc.
#ifndef FL_PIN_SPI_DATA_1
#define FL_PIN_SPI_DATA_1 1
#endif

/// @brief Default clock pin for clocked/SPI chipsets.
#ifndef FL_PIN_SPI_CLOCK_1
#define FL_PIN_SPI_CLOCK_1 2
#endif
