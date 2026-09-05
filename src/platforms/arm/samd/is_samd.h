// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file is_samd.h
/// @brief SAMD/SAME platform detection macros for FastLED
///
/// This header provides detection for Microchip SAMD (ARM Cortex-M0+/M4) platforms.
/// Detection keys off CPU-specific macros (e.g. `__SAMD21G18A__`), which name an
/// exact part and are supplied by the board manifest itself.
///
/// `ARDUINO_ARCH_SAMD` is accepted as an *additional* way to recognize a SAMD
/// board, never as a requirement. It is injected by PlatformIO's Arduino builder
/// script rather than by the board manifest, so build systems that consume the
/// manifest directly (fbuild) never define it. Requiring it here meant an
/// unmistakable `-D__SAMD21G18A__` was ignored and the build fell through to the
/// `#error` in `platforms/pin.h`. This matches the sibling detection headers:
/// `is_nrf52.h` and `is_apollo3.h` likewise treat `ARDUINO_ARCH_*` as one
/// alternative among several, and `is_teensy.h`/`is_rp.h`/`is_sam.h` do not
/// consult it at all.

// SAMD21 (ARM Cortex-M0+, 48 MHz)
#if defined(__SAMD21__) || defined(__SAMD21G18A__) || \
    defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
    defined(__SAMD21E18A__)
#define FL_IS_SAMD21
#endif

// SAMD51/SAME51 (ARM Cortex-M4F, 120 MHz)
#if defined(__SAMD51__) || defined(__SAMD51G19A__) || \
    defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
    defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#define FL_IS_SAMD51
#endif

// General SAMD platform (any SAMD board)
#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51) || defined(ARDUINO_ARCH_SAMD)
#define FL_IS_SAMD
#endif
