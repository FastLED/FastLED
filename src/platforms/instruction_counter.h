#pragma once

// ok no namespace fl

/// @file platforms/instruction_counter.h
/// Dispatch for fl::InstructionCounter (FastLED#5376): a retired-instruction
/// counter for profiling. ESP32-C3/C6/H2 count with Espressif's custom RISC-V
/// performance counter; every other target gets the null counter.
///
/// RMT4 diagnostics read the same counter as a cycle counter from an ISR
/// (`__clock_cycles()` in channel_driver_rmt4.cpp.hpp); a counted interval
/// would hand them instructions, so those builds get the null counter too.

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32H2)
// Defines FL_RMT4_DIAGNOSTICS (0 unless the build enables it), so the choice
// below does not depend on include order.
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_config.h"
#if FL_RMT4_DIAGNOSTICS
#include "platforms/shared/instruction_counter_null.h"
#else
#include "platforms/esp/32/core/instruction_counter_riscv.h"
#endif
#else
#include "platforms/shared/instruction_counter_null.h"
#endif
