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
    uint32_t mIsrCount;
    uint32_t mChunksQueued;
    uint32_t mChunksCompleted;
    uint32_t mBytesTotal;
    uint32_t mBytesTransmitted;
    uint32_t mErrorCode;
    bool mTransmissionActive;
};

} // namespace detail
} // namespace fl
