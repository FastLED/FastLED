/// @file parlio_isr_context.h
/// @brief ISR context and ISR-safe utilities for PARLIO driver
///
/// ⚠️ ⚠️ ⚠️  CRITICAL ISR SAFETY RULES - READ BEFORE MODIFYING ⚠️ ⚠️ ⚠️
///
/// This header contains code that runs in INTERRUPT CONTEXT with EXTREMELY strict constraints:
///
/// 1. ❌ ABSOLUTELY NO LOGGING (FL_LOG_PARLIO, FL_WARN, FL_ERROR, printf, etc.)
///    - Logging can cause watchdog timeouts, crashes, or system instability
///    - Even "ISR-safe" logging can introduce unacceptable latency
///    - If you need to debug, use GPIO toggling or counters instead
///
/// 2. ❌ NO BLOCKING OPERATIONS (mutex, delay, heap allocation, etc.)
///    - ISRs must complete in microseconds, not milliseconds
///    - Any blocking operation will crash the system
///
/// 3. ✅ ONLY USE ISR-SAFE FREERTOS FUNCTIONS (xSemaphoreGiveFromISR, etc.)
///    - Always pass higherPriorityTaskWoken and return its value
///    - Never use non-ISR variants (xSemaphoreGive, etc.)
///
/// 4. ✅ MINIMIZE EXECUTION TIME
///    - Keep ISR as short as possible (ideally <10µs)
///    - Defer complex work to main thread via flags/semaphores
///
/// 5. ✅ ALL ISR FUNCTIONS MUST HAVE FL_IRAM ATTRIBUTE
///    - Ensures code is placed in IRAM (not flash)
///    - Prevents cache misses during ISR execution
///
/// If the system crashes after you modify this code:
/// - First suspect: Did you add logging?
/// - Second suspect: Did you add blocking operations?
/// - Third suspect: Did you increase execution time?
/// - Fourth suspect: Did you forget FL_IRAM on a new function?
///
/// ============================================================================

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"

#include "fl/stl/atomic.h"
#include "fl/stl/isr/memcpy.h"
#include "fl/stl/align.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

//=============================================================================
// ISR Context Structure
//=============================================================================

/// @brief Cache-aligned ISR context for PARLIO transmission state
///
/// Memory Synchronization Model:
/// - ISR writes completion flags with release ordering.
/// - Main thread reads completion flags with acquire ordering.
/// - Non-atomic counters are read after completion has been observed.
struct FL_ALIGNAS(64) ParlioIsrContext {
    // === ISR-shared fields (ISR writes, main reads) ===
    fl::atomic_bool mStreamComplete;
    fl::atomic_bool mTransmitting;
    volatile size_t mCurrentByte;
    volatile size_t mRingReadIdx;    // 0-2 (for 3-buffer ring)
    volatile size_t mRingWriteIdx;   // 0-2 (for 3-buffer ring)
    volatile size_t mRingCount;      // 0-3 (distinguishes full vs empty)
    fl::atomic_bool mRingError;
    fl::atomic_bool mHardwareIdle;
    volatile u32 mUnderrunCount;     // Ring emptied while source bytes remained
    volatile size_t mNextByteOffset; // Next byte offset in source data (Worker function updates)

    // === Fields read after completion has been observed ===
    size_t mTotalBytes;
    size_t mNumLanes;
    u32 mIsrCount;
    u32 mBytesTransmitted;
    u32 mChunksCompleted;
    fl::atomic_bool mTransmissionActive;
    u64 mEndTimeUs;

    // === Debug Counters (volatile for ISR access) ===
    volatile u32 mDebugTxDoneCount;       // Count of txDoneCallback invocations
    volatile u32 mDebugWorkerIsrCount;    // Count of workerIsrCallback invocations
    volatile u64 mDebugLastTxDoneTime;    // esp_timer_get_time() at last txDone
    volatile u64 mDebugLastWorkerIsrTime; // esp_timer_get_time() at last worker ISR

    ParlioIsrContext() FL_NOEXCEPT
        : mStreamComplete(false), mTransmitting(false), mCurrentByte(0),
          mRingReadIdx(0), mRingWriteIdx(0), mRingCount(0), mRingError(false),
          mHardwareIdle(false), mUnderrunCount(0), mNextByteOffset(0),
          mTotalBytes(0), mNumLanes(0), mIsrCount(0),
          mBytesTransmitted(0), mChunksCompleted(0),
          mTransmissionActive(false), mEndTimeUs(0),
          mDebugTxDoneCount(0), mDebugWorkerIsrCount(0),
          mDebugLastTxDoneTime(0), mDebugLastWorkerIsrTime(0) {
    }
};

} // namespace detail
} // namespace fl
