/// @file parlio_debug.h
/// @brief Debug metrics and utilities for PARLIO engine

#pragma once

// IWYU pragma: private

#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

//=============================================================================
// Debug Metrics Structure
//=============================================================================

struct ParlioDebugMetrics {
    u64 mStartTimeUs;
    u64 mEndTimeUs;
    u32 mIsrCount;
    u32 mChunksQueued;
    u32 mChunksCompleted;
    u32 mBytesTotal;
    u32 mBytesTransmitted;
    u32 mErrorCode;
    bool mTransmissionActive;
};

} // namespace detail
} // namespace fl
