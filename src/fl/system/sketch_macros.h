#pragma once

// Pull in platform detection BEFORE the tier classification fires. Without
// this, FL_IS_ARM_LPC / FL_IS_TEENSY_LC / FL_IS_STM32_F1 etc. -- the gates
// in the Low-tier branch below -- are undefined when sketch_macros.h is
// transitively included via fl/system/engine_events.h before any other
// platform-aware include. Result: LPC builds get FL_PLATFORM_HAS_LARGE_MEMORY=1
// (wrong) and pull in FASTLED_HAS_ENGINE_EVENTS code that lacks symbol
// definitions -- link fails. Pulling is_platform.h here makes the detection
// canonical regardless of include order.
#include "platforms/is_platform.h"

// Four-tier platform-memory classification (FastLED #3000).
//
// Canonical names:
//   FL_PLATFORM_HAS_TINY_MEMORY   — 1 on parts with <=1KB SRAM
//                                   (classic ATtiny <=512B, ATtiny1604 1KB,
//                                    select modern tinyAVR 0/1-series with
//                                    <=1KB SRAM — see FL_IS_AVR_ATTINY_TINY_MEMORY
//                                    in is_avr.h; 2-series parts with 2KB+ are
//                                    excluded)
//   FL_PLATFORM_HAS_LARGE_MEMORY  — 1 on the "high" tier (Apollo3, nRF52,
//                                   SAMD21, generic ARM)
//   FL_PLATFORM_HAS_HUGE_MEMORY   — 1 on the "huge" tier (ESP32, Teensy
//                                   3.5/3.6/4.x, RP2040/RP2350, SAMD51,
//                                   STM32F4+/H7, native/WASM)
//
// Tier truth table:
//   Tiny: TINY=1, LARGE=0, HUGE=0  (<=1KB SRAM parts)
//   Low:  TINY=0, LARGE=0, HUGE=0  (AVR Uno/Nano/Leonardo, ATtiny 2-3KB,
//                                   ESP8266, Teensy LC/3.0/3.1/3.2, STM32F1,
//                                   Renesas UNO, LPC8xx)
//   High: TINY=0, LARGE=1, HUGE=0
//   Huge: TINY=0, LARGE=1, HUGE=1
//
// Invariants:
//   HUGE=1 implies LARGE=1. LARGE=0 implies HUGE=0.
//   TINY=1 implies LARGE=0 and HUGE=0.
//
// Backward compatibility (#3000):
//   The names `SKETCH_HAS_TINY_MEMORY`, `SKETCH_HAS_LARGE_MEMORY`,
//   `SKETCH_HAS_HUGE_MEMORY` (and `_OVERRIDDEN` variants, plus the older
//   `SKETCH_HAS_LOTS_OF_MEMORY` / `SKETCH_HAS_VERY_LARGE_MEMORY` aliases)
//   are preserved as deprecated aliases so external user sketches keep
//   compiling. Internal FastLED code is migrating to the
//   `FL_PLATFORM_HAS_*` names over a series of follow-up PRs; both names
//   work identically until that migration completes. Either name can be
//   used as a build-flag override (e.g. `-DSKETCH_HAS_LARGE_MEMORY=1` or
//   `-DFL_PLATFORM_HAS_LARGE_MEMORY=1`).
//
// Override mechanism: define the macro to 0 or 1 in build flags or
// before including FastLED. The header detects the override and sets
// the corresponding `_OVERRIDDEN` flag, which `platforms/*/compile_test.hpp`
// uses to skip the platform-default sanity assertion.

// =============================================================================
// Tier 1: tiny (<=1KB SRAM)
// =============================================================================
//
// Detect BEFORE computing LARGE/HUGE so the invariant TINY=1 → LARGE=0,
// HUGE=0 holds for the platform-default paths.
//
// `FL_IS_AVR_ATTINY_TINY_MEMORY` is defined in `platforms/avr/is_avr.h`
// (included via `platforms/is_platform.h` upstream in FastLED.h).
#if defined(FL_PLATFORM_HAS_TINY_MEMORY) || defined(SKETCH_HAS_TINY_MEMORY)
  // User-supplied override. Accept either spelling.
  #ifndef FL_PLATFORM_HAS_TINY_MEMORY
    #define FL_PLATFORM_HAS_TINY_MEMORY SKETCH_HAS_TINY_MEMORY
  #endif
  #define FL_PLATFORM_HAS_TINY_MEMORY_OVERRIDDEN 1
#else
  #if defined(FL_IS_AVR_ATTINY_TINY_MEMORY)
    #define FL_PLATFORM_HAS_TINY_MEMORY 1
  #else
    #define FL_PLATFORM_HAS_TINY_MEMORY 0
  #endif
#endif

// =============================================================================
// Tier 3: large / high (>= 256KB flash, "high" tier)
// =============================================================================
#if defined(FL_PLATFORM_HAS_LARGE_MEMORY) || defined(SKETCH_HAS_LARGE_MEMORY)
  // User-supplied override. Accept either spelling.
  #ifndef FL_PLATFORM_HAS_LARGE_MEMORY
    #define FL_PLATFORM_HAS_LARGE_MEMORY SKETCH_HAS_LARGE_MEMORY
  #endif
  #define FL_PLATFORM_HAS_LARGE_MEMORY_OVERRIDDEN 1
#else
  #if defined(FL_IS_AVR) \
    || defined(__AVR_ATtiny85__) \
    || defined(__AVR_ATtiny88__) \
    || defined(__AVR_ATmega32U4__) \
    || defined(ARDUINO_attinyxy6) \
    || defined(ARDUINO_attinyxy4) \
    || defined(FL_IS_TEENSY_LC) \
    || defined(FL_IS_TEENSY_30) \
    || defined(FL_IS_TEENSY_31) \
    || defined(FL_IS_TEENSY_32) \
    || defined(FL_IS_STM32_F1) \
    || defined(FL_IS_ESP8266) \
    || defined(ARDUINO_ARCH_RENESAS_UNO) \
    || defined(ARDUINO_BLUEPILL_F103C8) \
    || defined(FL_IS_ARM_LPC)
    #define FL_PLATFORM_HAS_LARGE_MEMORY 0
  #else
    #define FL_PLATFORM_HAS_LARGE_MEMORY 1
  #endif
#endif

// =============================================================================
// Tier 4: huge (>= 256KB RAM, "huge" tier)
// =============================================================================
//
// ESP32 (all), Teensy 4.x/3.5/3.6, RP2040/RP2350, SAMD51, STM32F4+/H7,
// native/WASM.
#if defined(FL_PLATFORM_HAS_HUGE_MEMORY) || defined(SKETCH_HAS_HUGE_MEMORY)
  // User-supplied override. Accept either spelling.
  #ifndef FL_PLATFORM_HAS_HUGE_MEMORY
    #define FL_PLATFORM_HAS_HUGE_MEMORY SKETCH_HAS_HUGE_MEMORY
  #endif
  #define FL_PLATFORM_HAS_HUGE_MEMORY_OVERRIDDEN 1
#else
  #if defined(FL_IS_ESP32) \
    || defined(FL_IS_TEENSY_35) \
    || defined(FL_IS_TEENSY_36) \
    || defined(FL_IS_TEENSY_4X) \
    || defined(ARDUINO_ARCH_RP2040) \
    || defined(PICO_RP2040) \
    || defined(PICO_RP2350) \
    || defined(__SAMD51__) \
    || defined(STM32F4xx) || defined(STM32H7xx) || defined(ARDUINO_GIGA) \
    || defined(FASTLED_STUB_IMPL) \
    || defined(__EMSCRIPTEN__)
    #define FL_PLATFORM_HAS_HUGE_MEMORY 1
  #else
    #define FL_PLATFORM_HAS_HUGE_MEMORY 0
  #endif
#endif

// =============================================================================
// Invariant enforcement
// =============================================================================
#if FL_PLATFORM_HAS_HUGE_MEMORY && !FL_PLATFORM_HAS_LARGE_MEMORY
  #error "FL_PLATFORM_HAS_HUGE_MEMORY=1 requires FL_PLATFORM_HAS_LARGE_MEMORY=1"
#endif
#if FL_PLATFORM_HAS_TINY_MEMORY && FL_PLATFORM_HAS_LARGE_MEMORY
  #error "FL_PLATFORM_HAS_TINY_MEMORY=1 is incompatible with FL_PLATFORM_HAS_LARGE_MEMORY=1"
#endif

// =============================================================================
// Backward-compat aliases (FastLED #3000)
// =============================================================================
//
// The legacy `SKETCH_HAS_*_MEMORY` names mirror the canonical
// `FL_PLATFORM_HAS_*_MEMORY` defines verbatim. Both names will continue
// to evaluate identically; new code should prefer the `FL_PLATFORM_HAS_*`
// names. The `_OVERRIDDEN` variants are also mirrored for parity with
// the platform `compile_test.hpp` files that key off them.
//
// `#ifndef` guards protect the case where the user already supplied
// `SKETCH_HAS_*` (which we picked up above as an override) — preventing
// a redefinition warning.
#ifndef SKETCH_HAS_TINY_MEMORY
  #define SKETCH_HAS_TINY_MEMORY FL_PLATFORM_HAS_TINY_MEMORY
#endif
#ifndef SKETCH_HAS_LARGE_MEMORY
  #define SKETCH_HAS_LARGE_MEMORY FL_PLATFORM_HAS_LARGE_MEMORY
#endif
#ifndef SKETCH_HAS_HUGE_MEMORY
  #define SKETCH_HAS_HUGE_MEMORY FL_PLATFORM_HAS_HUGE_MEMORY
#endif

#ifdef FL_PLATFORM_HAS_TINY_MEMORY_OVERRIDDEN
  #ifndef SKETCH_HAS_TINY_MEMORY_OVERRIDDEN
    #define SKETCH_HAS_TINY_MEMORY_OVERRIDDEN 1
  #endif
#endif
#ifdef FL_PLATFORM_HAS_LARGE_MEMORY_OVERRIDDEN
  #ifndef SKETCH_HAS_LARGE_MEMORY_OVERRIDDEN
    #define SKETCH_HAS_LARGE_MEMORY_OVERRIDDEN 1
  #endif
#endif
#ifdef FL_PLATFORM_HAS_HUGE_MEMORY_OVERRIDDEN
  #ifndef SKETCH_HAS_HUGE_MEMORY_OVERRIDDEN
    #define SKETCH_HAS_HUGE_MEMORY_OVERRIDDEN 1
  #endif
#endif

// Older spellings still seen in some user sketches.
#ifndef SKETCH_HAS_LOTS_OF_MEMORY
  #define SKETCH_HAS_LOTS_OF_MEMORY FL_PLATFORM_HAS_LARGE_MEMORY
#endif
#ifndef SKETCH_HAS_VERY_LARGE_MEMORY
  #define SKETCH_HAS_VERY_LARGE_MEMORY FL_PLATFORM_HAS_HUGE_MEMORY
#endif

// =============================================================================
// Misc utilities (kept as-is)
// =============================================================================
#ifndef SKETCH_STRINGIFY
#define SKETCH_STRINGIFY_HELPER(x) #x
#define SKETCH_STRINGIFY(x) SKETCH_STRINGIFY_HELPER(x)
#endif

// SKETCH_HALT and SKETCH_HALT_OK macros have been removed.
// They caused watchdog timer resets on ESP32-C6 due to the infinite while(1) loop
// preventing loop() from returning.
//
// Replacement: Use a static flag to run tests once:
//
// Pattern:
//   static bool tests_run = false;
//
//   void loop() {
//       if (tests_run) {
//           delay(1000);
//           return;
//       }
//       tests_run = true;
//
//       // ... your test code here ...
//       FL_PRINT("Tests complete");
//   }
//
// This allows loop() to return normally, preventing watchdog timer resets.
