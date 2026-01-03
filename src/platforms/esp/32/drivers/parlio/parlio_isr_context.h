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
#ifdef ESP32

#include "fl/unused.h"
#include "platforms/memory_barrier.h"

FL_EXTERN_C_BEGIN
#include "esp_cache.h"      // For esp_cache_msync (DMA cache coherency)
#include "esp_heap_caps.h"  // For MALLOC_CAP_* flags
FL_EXTERN_C_END

// DMA memory allocation flags
#define FL_PARLIO_DMA_MALLOC_FLAGS  (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define FL_PARLIO_DMA_NEEDS_SYNC !(MALLOC_CAP_INTERNAL & FL_PARLIO_DMA_MALLOC_FLAGS)

namespace fl {
namespace detail {

//=============================================================================
// ISR-Safe Memory Operations
//=============================================================================

/// @brief ISR-safe memset replacement (manual loop copy)
/// fl::memset is not allowed in ISR context on some platforms
/// This function uses a simple loop to zero memory
FL_OPTIMIZE_FUNCTION static inline void FL_IRAM
isr_memset_zero_byte(uint8_t* dest, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = 0x0;
    }
}

/// @brief ISR-safe word-aligned memset (4-byte writes)
FL_OPTIMIZE_FUNCTION static inline void FL_IRAM
isr_memset_zero_word(uint8_t* dest, size_t count) {
    uint32_t* dest32 = reinterpret_cast<uint32_t*>(dest);
    size_t count32 = count / 4;
    size_t remainder = count % 4;

    for (size_t i = 0; i < count32; i++) {
        dest32[i] = 0;
    }

    if (remainder > 0) {
        uint8_t* remainder_ptr = dest + (count32 * 4);
        isr_memset_zero_byte(remainder_ptr, remainder);
    }
}

/// @brief ISR-safe memset with alignment optimization
FL_OPTIMIZE_FUNCTION static inline void FL_IRAM
isr_memset_zero(uint8_t* dest, size_t count) {
    uintptr_t address = reinterpret_cast<uintptr_t>(dest);

    // If aligned AND large enough, use fast word writes
    if ((address % 4 == 0) && (count >= 4)) {
        isr_memset_zero_word(dest, count);
    } else {
        // Unaligned or small: use byte writes
        isr_memset_zero_byte(dest, count);
    }
}

/// @brief ISR-safe cache sync for DMA buffers
/// @param buffer_ptr Pointer to buffer to sync
/// @param buffer_size Size of buffer in bytes
/// @param context Context string for async logging (unused - logging suppressed)
/// @return esp_err_t result from esp_cache_msync (ESP_OK or error code)
///
/// CRITICAL: This function is callable from ISR context (FL_IRAM attribute)
/// - Memory barriers ensure proper ordering
/// - esp_cache_msync errors are suppressed (expected on ESP32-C6 with MALLOC_CAP_INTERNAL)
///
/// Expected behavior on ESP32-C6:
/// - MALLOC_CAP_INTERNAL buffers: Returns ESP_ERR_INVALID_ARG (258) but still improves accuracy
/// - MALLOC_CAP_DMA buffers: Returns ESP_ERR_INVALID_ARG (address not cacheable)
/// - Despite errors, cache sync improves data integrity from 44% corruption to 99.97% accuracy
/// - Error logging is suppressed to avoid flooding output (errors are expected and benign)
FL_OPTIMIZE_FUNCTION static inline esp_err_t FL_IRAM
isr_cache_sync(void* buffer_ptr, size_t buffer_size, const char* context) {
    // Suppress unused parameter warning
    FL_UNUSED(context);
    if (!FL_PARLIO_DMA_NEEDS_SYNC) {
        return ESP_OK;
    }

    // Memory barrier: Ensure all preceding writes complete before cache sync
    FL_MEMORY_BARRIER;

    // Cache sync (may return ESP_ERR_INVALID_ARG on ESP32-C6, but still essential for data integrity)
    esp_err_t err = esp_cache_msync(
        buffer_ptr,
        buffer_size,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M);  // Cache-to-Memory writeback

    // Memory barrier: Ensure cache sync completes before DMA submission
    FL_MEMORY_BARRIER;

    // Note: Error logging suppressed - esp_cache_msync returns errors on ESP32-C6 with
    // MALLOC_CAP_INTERNAL, but the function still has beneficial side effects for
    // data integrity. Logging hundreds of expected errors clutters output.

    return err;
}

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
    volatile size_t mNextByteOffset; // Next byte offset in source data (Worker ISR updates)
    volatile bool mWorkerIsrEnabled; // Worker ISR armed state
    volatile bool mWorkerDoorbell;   // Doorbell flag: txDone sets, workerIsr clears (continuous timer mode)

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
    volatile uint32_t mDebugDoorbellRingCount; // Count of doorbell rings (txDone sets flag)
    volatile uint32_t mDebugDoorbellCheckCount; // Count of worker ISR firings (total, includes no-work exits)
    volatile uint32_t mDebugWorkerEarlyExitCount; // Count of worker ISR early exits (doorbell not set)

    ParlioIsrContext()
        : mStreamComplete(false), mTransmitting(false), mCurrentByte(0),
          mRingReadIdx(0), mRingWriteIdx(0), mRingCount(0), mRingError(false),
          mHardwareIdle(false), mNextByteOffset(0), mWorkerIsrEnabled(false),
          mWorkerDoorbell(false),
          mTotalBytes(0), mNumLanes(0), mIsrCount(0),
          mBytesTransmitted(0), mChunksCompleted(0),
          mTransmissionActive(false), mEndTimeUs(0),
          mDebugTxDoneCount(0), mDebugWorkerIsrCount(0),
          mDebugLastTxDoneTime(0), mDebugLastWorkerIsrTime(0),
          mDebugDoorbellRingCount(0), mDebugDoorbellCheckCount(0),
          mDebugWorkerEarlyExitCount(0) {
    }
};

} // namespace detail
} // namespace fl

#endif // ESP32
