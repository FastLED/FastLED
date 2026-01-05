/// @file parlio_isr_context.h
/// @brief ISR context and ISR-safe utilities for PARLIO engine
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

#include "fl/compiler_control.h"

#include "fl/isr.h"
#include "fl/unused.h"
#include "platforms/memory_barrier.h"

namespace fl {
namespace detail {

//=============================================================================
// ISR Context Structure
//=============================================================================

/// @brief Cache-aligned ISR context for PARLIO transmission state
///
/// Memory Synchronization Model:
/// - ISR writes to volatile fields (mStreamComplete, mTransmitting, etc.)
/// - Main thread reads volatile fields directly (compiler ensures fresh read)
/// - After detecting mStreamComplete==true, main executes memory barrier
/// - Memory barrier ensures all ISR writes visible before reading non-volatile fields
struct alignas(64) ParlioIsrContext {
    // === Volatile Fields (ISR writes, main reads) ===
    volatile bool mStreamComplete;
    volatile bool mTransmitting;
    volatile size_t mCurrentByte;
    volatile size_t mRingReadIdx;    // 0-2 (for 3-buffer ring)
    volatile size_t mRingWriteIdx;   // 0-2 (for 3-buffer ring)
    volatile size_t mRingCount;      // 0-3 (distinguishes full vs empty)
    volatile bool mRingError;
    volatile bool mHardwareIdle;
    volatile size_t mNextByteOffset; // Next byte offset in source data (Worker function updates)

    // === Non-Volatile Fields (read after barrier only) ===
    size_t mTotalBytes;
    size_t mNumLanes;
    uint32_t mIsrCount;
    uint32_t mBytesTransmitted;
    uint32_t mChunksCompleted;
    bool mTransmissionActive;
    uint64_t mEndTimeUs;

    // === Debug Counters (volatile for ISR access) ===
    volatile uint32_t mDebugTxDoneCount;       // Count of txDoneCallback invocations
    volatile uint32_t mDebugWorkerIsrCount;    // Count of workerIsrCallback invocations
    volatile uint64_t mDebugLastTxDoneTime;    // esp_timer_get_time() at last txDone
    volatile uint64_t mDebugLastWorkerIsrTime; // esp_timer_get_time() at last worker ISR

    ParlioIsrContext()
        : mStreamComplete(false), mTransmitting(false), mCurrentByte(0),
          mRingReadIdx(0), mRingWriteIdx(0), mRingCount(0), mRingError(false),
          mHardwareIdle(false), mNextByteOffset(0),
          mTotalBytes(0), mNumLanes(0), mIsrCount(0),
          mBytesTransmitted(0), mChunksCompleted(0),
          mTransmissionActive(false), mEndTimeUs(0),
          mDebugTxDoneCount(0), mDebugWorkerIsrCount(0),
          mDebugLastTxDoneTime(0), mDebugLastWorkerIsrTime(0) {
    }
};

} // namespace detail
} // namespace fl
