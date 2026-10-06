#pragma once

// IWYU pragma: private
// ok no namespace fl - macro-only header

/// @file dmamem.h
/// @brief FL_DMAMEM: Teensy 4 DMAMEM placement without `used`.
///
/// The Teensy core defines `DMAMEM` as
/// `__attribute__((section(".dmabuffers"), used))`. The `used` keeps a
/// driver's DMA buffer in the image even when no linked code references it,
/// so every sketch paid for every driver's buffer in RAM2 (#4713). FL_DMAMEM
/// places the buffer in the same `.dmabuffers` section (OCRAM2, DMA-safe)
/// but lets the compiler/LTO drop it when its driver is not linked.

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)
#define FL_DMAMEM __attribute__((section(".dmabuffers")))
#else
#define FL_DMAMEM
#endif
