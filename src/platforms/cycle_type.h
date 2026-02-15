#pragma once

#ifndef __INC_FASTLED_PLATFORMS_CYCLE_TYPE_H
#define __INC_FASTLED_PLATFORMS_CYCLE_TYPE_H

/// @file platforms/cycle_type.h
/// Platform-specific typedef for CPU clock cycle counts
/// Used as template parameter for delaycycles<cycle_t CYCLES>()

#include "fl/int.h"

namespace fl {

/// Type for representing CPU clock cycle counts
/// - On AVR: int (16-bit signed) to minimize memory usage
/// - On other platforms: fl::i64 (64-bit signed) for high-frequency CPUs
#if defined(__AVR__)
typedef int cycle_t;
#else
typedef fl::i64 cycle_t;
#endif

} // namespace fl

#endif // __INC_FASTLED_PLATFORMS_CYCLE_TYPE_H
