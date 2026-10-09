#pragma once

/// @file clockless_encoders.h
/// @brief Link-on-use switch for the clockless half of unified parallel-IO
///        engines (#4793).
///
/// The parallel-IO rule (agents/docs/cpp-standards.md -> "Parallel-IO
/// Driver: Unified Clockless + SPI Engine") keeps one engine per peripheral
/// for both modes. On ESP32 / ESP32-S3 that engine is also the default SPI
/// bus, so an SPI-only sketch would otherwise link the whole clockless
/// encode pipeline (wave8/wave3 transposers, clockless peripheral init) for
/// code it can never run.
///
/// Every path that creates a clockless channel calls
/// `enableClocklessEncoders()`. The engines reach their clockless pipeline
/// only through state that this call installs, so a program that never
/// creates a clockless channel never references that code and
/// `--gc-sections` drops it. The engine and its Bus slot stay unified.

#include "fl/stl/noexcept.h"
#include "platforms/is_platform.h"

namespace fl {
namespace platforms {

#if defined(FL_IS_ESP32)
/// Install the clockless pipelines of this platform's unified engines.
/// Idempotent and cheap; called from every clockless channel construction.
void enableClocklessEncoders() FL_NO_EXCEPT;
#else
inline void enableClocklessEncoders() FL_NO_EXCEPT {}
#endif

}  // namespace platforms
}  // namespace fl
