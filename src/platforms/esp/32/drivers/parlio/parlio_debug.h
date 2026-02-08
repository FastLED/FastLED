/// @file parlio_debug.h
/// @brief Debug metrics and utilities for PARLIO engine

#pragma once

#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

//=============================================================================
// Debug Metrics Structure
//=============================================================================

struct ParlioDebugMetrics {
    uint64_t mStartTimeUs;
    uint64_t mEndTimeUs;
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
