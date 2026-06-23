// ok no namespace fl
#pragma once

// IWYU pragma: private

#ifndef __INC_FASTLED_PLATFORMS_RP2040_DELAYCYCLES_H
#define __INC_FASTLED_PLATFORMS_RP2040_DELAYCYCLES_H

#include "platforms/cycle_type.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

/// @file platforms/arm/rp/rp2040/delaycycles.h
/// RP2040 platform-specific cycle-accurate delay utilities

/// Forward declaration for delay_cycles_pico
/// Implementation is in delay.cpp which includes Arduino.h for Pico SDK access
void delay_cycles_pico(fl::u32 cycles) FL_NOEXCEPT;

#endif  // __INC_FASTLED_PLATFORMS_RP2040_DELAYCYCLES_H
