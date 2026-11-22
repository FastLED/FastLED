#pragma once

/// @file fl/system/instruction_counter.h
/// Counts the instructions this core retires over an interval, for profiling
/// (FastLED#5376). Wall time moves with code layout between builds; the
/// instruction count does not, so it judges small optimisations.
///
/// ```
/// fl::InstructionCounter counter;
/// counter.start();
/// const u32 t0 = micros();
/// FastLED.show();
/// const u32 dt = micros() - t0;
/// const u32 instructions = counter.stop(dt * cpu_mhz);
/// if (fl::InstructionCounter::kSupported) { report(instructions); }
/// ```
///
/// Supported on ESP32-C3/C6/H2; elsewhere kSupported is false and stop()
/// returns 0.

#include "platforms/instruction_counter.h"

namespace fl {
using InstructionCounter = platforms::InstructionCounter;
}  // namespace fl
