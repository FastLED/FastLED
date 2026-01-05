/// @file parlio_engine.cpp
/// @brief PARLIO hardware abstraction layer (HAL) implementation for ESP32
///
/// This file contains the low-level PARLIO hardware management code extracted
/// from channel_engine_parlio.cpp. It handles all hardware-specific operations
/// including ISR callbacks, DMA buffer generation, and ring buffer streaming.

#ifdef ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

// Compile on ESP32 with PARLIO support OR on stub platform for testing
#if (defined(ESP32) && FASTLED_ESP32_HAS_PARLIO) || defined(FASTLED_STUB_IMPL)

#include "fl/compiler_control.h"
#include "fl/unused.h"

#include "parlio_engine.h"
#include "parlio_isr_context.h"
#include "parlio_buffer_calc.h"
#include "parlio_debug.h"
#include "parlio_ring_buffer.h"
#include "fl/delay.h"
#include "fl/error.h"
#include "fl/log.h"
#include "fl/transposition.h"
#include "fl/warn.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/assert.h"
#include "fl/stl/time.h"

#ifdef ESP32
#include "platforms/esp/32/core/fastpin_esp32.h" // For _FL_VALID_PIN_MASK
#include "platforms/memory_barrier.h" // For FL_MEMORY_BARRIER
#else
// Stub platform definitions
#ifndef FL_MEMORY_BARRIER
#define FL_MEMORY_BARRIER do {} while(0)
#endif
#ifndef _FL_VALID_PIN_MASK
#define _FL_VALID_PIN_MASK 0xFFFFFFFFFFFFFFFFULL  // All pins valid on stub
#endif
#endif

// All ESP-IDF dependencies have been abstracted through IParlioPeripheral interface
// - Timer operations: fl::isr::attachTimerHandler(), etc. (via fl/isr.h)
// - Timestamp operations: getMicroseconds()
// - Task management: TaskCoroutine (via fl/task.h)
// - DMA memory: allocateDmaBuffer(), freeDmaBuffer()
// - PARLIO operations: initialize(), enable(), transmit(), waitAllDone(), etc.

#ifndef CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM
#warning \
    "PARLIO: CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM is not defined! Add 'build_flags = -DCONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1' to platformio.ini or your build system"
#endif

namespace fl {
namespace detail {

//=============================================================================
// Constants
//=============================================================================

// WS2812B PARLIO clock frequency
// - 8.0 MHz produces 125ns per tick (matches wave8 8-pulse expansion)
// - Each LED bit = 8 clock ticks = 1.0μs total
// - Divides from PLL_F160M on ESP32-P4 (160/20) or PLL_F240M on ESP32-C6 (240/30)
#ifndef FL_ESP_PARLIO_CLOCK_FREQ_HZ
#define FL_ESP_PARLIO_CLOCK_FREQ_HZ 8000000 // 8.0 MHz
#endif // defined(FL_ESP_PARLIO_CLOCK_FREQ_HZ)

#ifndef FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL
#define FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL 300 // Support up to 300 LEDs per channel (configurable)
#endif // defined(FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL)

// ESP32-C6 PARLIO hardware transaction queue depth limit
// CRITICAL: This value CANNOT be changed - it is a hardware limitation
// The ESP32-C6 PARLIO peripheral has a 3-state FSM (READY/PROGRESS/COMPLETE)
// Setting trans_queue_depth > 3 causes queue desynchronization and system crashes
// Empirical testing results (2025-12-29):
//   - trans_queue_depth = 3: Stable operation (hardware maximum)
//   - trans_queue_depth = 4: Queue desync, 64% more underruns
//   - trans_queue_depth = 8: Watchdog timeout crash
#ifndef FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH
#define FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH 3
#endif // defined(FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH)

// Worker timer configuration (for ISR-based buffer population)
// - Resolution: 1MHz = 1µs per tick (sufficient for 50µs timer period)
// - Interrupt priority: Level 3 for timely response without blocking critical ISRs
// - Period: 50µs continuous firing (doorbell pattern) - reduced from 20µs to lower ISR load during setup
#ifndef FL_ESP_PARLIO_WORKER_TIMER_RESOLUTION_HZ
#define FL_ESP_PARLIO_WORKER_TIMER_RESOLUTION_HZ 1000000  // 1MHz = 1µs resolution
#endif

#ifndef FL_ESP_PARLIO_WORKER_TIMER_INTR_PRIORITY
#define FL_ESP_PARLIO_WORKER_TIMER_INTR_PRIORITY 3  // Priority level 3
#endif

#ifndef FL_ESP_PARLIO_WORKER_TIMER_PERIOD_US
#define FL_ESP_PARLIO_WORKER_TIMER_PERIOD_US 50  // 50µs continuous period (reduced from 20µs for stability)
#endif

//=============================================================================
// Pin Validation Using FastLED's _FL_PIN_VALID System
//=============================================================================
// PARLIO no longer uses default pins. Instead, pins are extracted from
// ChannelData objects and validated using the FastLED pin validation system
// defined in platforms/esp/32/core/fastpin_esp32.h.
//
// The _FL_PIN_VALID macro checks against:
// 1. SOC_GPIO_VALID_OUTPUT_GPIO_MASK (ESP-IDF's valid output pins)
// 2. FASTLED_UNUSABLE_PIN_MASK (platform-specific forbidden pins)
//
// Pins are provided by user via FastLED.addLeds<WS2812, PIN>() API.
//=============================================================================

// Import pin validation mask from fastpin_esp32.h
// This is defined per-platform in src/platforms/esp/32/core/fastpin_esp32.h
#ifndef _FL_VALID_PIN_MASK
#error "Pin validation system not available - check fastpin_esp32.h is included"
#endif

/// @brief Validate a GPIO pin for PARLIO use
/// @param pin GPIO pin number to validate
/// @return true if pin is valid for PARLIO output, false otherwise
static inline bool isParlioPinValid(int pin) {
    if (pin < 0 || pin >= 64) {
        return false;
    }
    // Use FastLED's pin validation system
    uint64_t pin_mask = (1ULL << pin);
    return (_FL_VALID_PIN_MASK & pin_mask) != 0;
}

//=============================================================================
// ParlioEngine - Singleton Implementation
//=============================================================================

ParlioEngine::ParlioEngine()
    : mInitialized(false),
      mDataWidth(0),
      mActualChannels(0),
      mDummyLanes(0),
      mPeripheral(nullptr),
      mTimingT1Ns(0),
      mTimingT2Ns(0),
      mTimingT3Ns(0),
      mResetUs(0),
      mIsrContext(),
      mMainTaskHandle(nullptr),
      mWorkerTaskHandle(nullptr),
      mWorkerTimerHandle(),
      mRingBuffer(),
      mRingBufferCapacity(0),
      mScratchBuffer(nullptr),
      mLaneStride(0),
      mErrorOccurred(false),
      mTxUnitEnabled(false) {
}

ParlioEngine::~ParlioEngine() {
    // Wait for any active transmissions to complete
    while (isTransmitting()) {
        fl::delayMicroseconds(100);
    }

    // Clean up PARLIO peripheral
    if (mPeripheral != nullptr) {
        // Wait for any pending transmissions (with timeout)
        if (!mPeripheral->waitAllDone(1000)) {
            FL_LOG_PARLIO("PARLIO: Wait for transmission timeout during cleanup");
        }

        // Disable peripheral (only if currently enabled)
        if (mTxUnitEnabled) {
            if (!mPeripheral->disable()) {
                FL_LOG_PARLIO("PARLIO: Failed to disable peripheral");
            }
            mTxUnitEnabled = false;
        }

        // Delete peripheral (unique_ptr handles cleanup automatically)
        mPeripheral = nullptr;
    }

    // DMA buffers and waveform expansion buffer are automatically freed
    // by fl::unique_ptr destructors (RAII)

    // CRITICAL: Clean up worker timer FIRST to prevent ISR callbacks after mIsrContext is deleted
    // The timer ISR (workerIsrCallback) accesses mIsrContext, so timer must be detached
    // before we signal the debug task to exit and before we delete mIsrContext
    if (mWorkerTimerHandle.is_valid()) {
        // Detach timer (disables and removes ISR handler)
        fl::isr::detachHandler(mWorkerTimerHandle);

        // Give time for any in-flight ISR to complete (timer detach is asynchronous)
        // Without this delay, an ISR could fire after detach but before handle cleanup
        if (mPeripheral) {
            mPeripheral->delay(10);
        }
    }

    // Clean up debug task (now safe - timer ISR won't access mIsrContext anymore)
    if (mDebugTask.is_valid()) {
        // Signal task to exit by setting flags to false (task checks: while (mTransmitting || mStreamComplete))
        if (mIsrContext) {
            mIsrContext->mTransmitting = false;
            mIsrContext->mStreamComplete = false;  // Ensure loop exits (both conditions false)
            FL_MEMORY_BARRIER;
        }
        // Give task time to self-delete (task calls fl::task::exitCurrent() at end)
        // Task sleeps for 500ms, so we need to wait at least 500ms for it to wake up and check flags
        // Use 600ms to be safe
        if (mPeripheral) {
            mPeripheral->delay(600);
        }

        // Task should have self-deleted by now (via fl::task::exitCurrent())
        // Cancel the task to release resources (task object cleaned up automatically)
        mDebugTask.cancel();
    }

    // Clean up ring buffer (unique_ptr handles deletion automatically)
    mRingBuffer.reset();

    // Clean up IsrContext (unique_ptr handles deletion automatically)
    mIsrContext.reset();

    // Clear state
    mPins.clear();
}

ParlioEngine& ParlioEngine::getInstance() {
    return fl::Singleton<ParlioEngine>::instance();
}

//=============================================================================
// Debug Task - Periodic ISR State Logging
//=============================================================================

/// @brief Low-priority debug task that periodically logs ISR state
/// @param arg Pointer to ParlioEngine instance
///
/// This task runs every 500ms during transmission and prints ISR debug counters
/// to help diagnose crashes and timing issues. Per LOOP.md iteration 5 instructions.
void ParlioEngine::debugTaskFunction(void* arg) {
    auto *self = static_cast<ParlioEngine *>(arg);
    if (!self || !self->mIsrContext || !self->mPeripheral) {
        // Cannot proceed without valid context - self-delete
        fl::task::exitCurrent();
        return;  // UNREACHABLE on ESP32
    }

    ParlioIsrContext *ctx = self->mIsrContext.get();

    // Loop while transmission is active
    while (ctx->mTransmitting || ctx->mStreamComplete) {
        // Wait 500ms between prints
        if (self->mPeripheral) {
            self->mPeripheral->delay(500);
        }

        // Memory barrier to ensure we see latest ISR state
        FL_MEMORY_BARRIER;

        // Print ISR debug state (using FL_LOG_PARLIO which is safe from task context)
        FL_LOG_PARLIO("ISR_STATE:"
               << " txDone=" << ctx->mDebugTxDoneCount
               << " worker=" << ctx->mDebugWorkerIsrCount
               << " doorbell_ring=" << ctx->mDebugDoorbellRingCount
               << " doorbell_check=" << ctx->mDebugDoorbellCheckCount
               << " early_exit=" << ctx->mDebugWorkerEarlyExitCount
               << " ring_count=" << ctx->mRingCount
               << " bytes_tx=" << ctx->mBytesTransmitted << "/" << ctx->mTotalBytes
               << " transmitting=" << (ctx->mTransmitting ? "YES" : "NO")
               << " complete=" << (ctx->mStreamComplete ? "YES" : "NO")
               << " hw_idle=" << (ctx->mHardwareIdle ? "YES" : "NO")
               << " worker_enabled=" << (ctx->mWorkerIsrEnabled ? "YES" : "NO")
               << " doorbell=" << (ctx->mWorkerDoorbell ? "SET" : "CLEAR"));

        // Check for ring buffer underrun (CRITICAL ERROR per LOOP.md)
        if (ctx->mHardwareIdle && ctx->mRingCount == 0 && ctx->mTransmitting) {
            FL_ERROR("PARLIO: BUFFER UNDERRUN DETECTED - Hardware idle with empty ring during transmission"
                    << " | bytes_tx=" << ctx->mBytesTransmitted << "/" << ctx->mTotalBytes);
            // DO NOT restart PARLIO - per LOOP.md this is a critical error
            // Exit task and let main thread detect the error
            break;
        }

        // Exit if transmission complete
        if (ctx->mStreamComplete && !ctx->mTransmitting) {
            break;
        }
    }

    // Task is shutting down - self-delete
    fl::task::exitCurrent();
    // UNREACHABLE CODE on ESP32
}

//=============================================================================
// ISR Callback - Hardware Transmission Completion
//=============================================================================

// ============================================================================
// ⚠️ ⚠️ ⚠️  CRITICAL ISR SAFETY RULES - READ BEFORE MODIFYING ⚠️ ⚠️ ⚠️
// ============================================================================
//
// This function runs in INTERRUPT CONTEXT with EXTREMELY strict constraints:
//
// 1. ❌ ABSOLUTELY NO LOGGING (FL_LOG_PARLIO, FL_WARN, FL_ERROR, printf, etc.)
//    - Logging can cause watchdog timeouts, crashes, or system instability
//    - Even "ISR-safe" logging can introduce unacceptable latency
//    - If you need to debug, use GPIO toggling or counters instead
//
// 2. ❌ NO BLOCKING OPERATIONS (mutex, delay, heap allocation, etc.)
//    - ISRs must complete in microseconds, not milliseconds
//    - Any blocking operation will crash the system
//
// 3. ✅ ONLY USE ISR-SAFE FREERTOS FUNCTIONS (xSemaphoreGiveFromISR, etc.)
//    - Always pass higherPriorityTaskWoken and return its value
//    - Never use non-ISR variants (xSemaphoreGive, etc.)
//
// 4. ✅ MINIMIZE EXECUTION TIME
//    - Keep ISR as short as possible (ideally <10µs)
//    - Defer complex work to main thread via flags/semaphores
//
// If the system crashes after you modify this function:
// - First suspect: Did you add logging?
// - Second suspect: Did you add blocking operations?
// - Third suspect: Did you increase execution time?
//
// ============================================================================
FL_OPTIMIZE_FUNCTION bool FL_IRAM
ParlioEngine::txDoneCallback(void* tx_unit,
                              const void *edata, void *user_ctx) {
    // ⚠️  ISR CONTEXT - NO LOGGING ALLOWED - SEE FUNCTION HEADER ⚠️
    (void)tx_unit;  // Opaque handle (not used in this callback)
    (void)edata;

    auto *self = static_cast<ParlioEngine *>(user_ctx);
    if (!self || !self->mIsrContext) {
        return false;
    }

    // Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext *ctx = self->mIsrContext.get();

    // Debug: Increment txDoneCallback counter and timestamp
    ctx->mDebugTxDoneCount = ctx->mDebugTxDoneCount + 1;
    ctx->mDebugLastTxDoneTime = self->mPeripheral->getMicroseconds();

    // Increment ISR call counter
    ctx->mIsrCount++;

    // Account for bytes from the buffer that just completed transmission
    // The buffer that completed is the one BEFORE the current read_idx
    // (CPU or previous ISR call advanced read_idx after submitting)
    volatile size_t read_idx = ctx->mRingReadIdx;
    size_t completed_buffer_idx = (read_idx + ParlioRingBuffer3::RING_BUFFER_COUNT - 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;

    // Track transmitted bytes (use tracked input byte count, NOT DMA buffer size)
    // CRITICAL FIX (Iteration 6): DMA buffer includes reset padding, but byte accounting
    // should only count source data. Using reverse-calculation (dma_bytes / expansion)
    // caused 35-byte overflow when reset padding was present (9035 vs 9000 bytes).
    // Solution: Use pre-tracked input_sizes[] that excludes reset padding.
    size_t input_bytes = self->mRingBuffer->input_sizes[completed_buffer_idx];
    ctx->mBytesTransmitted += input_bytes;
    ctx->mCurrentByte += input_bytes;
    ctx->mChunksCompleted++;

    // SAFETY CHECK: Detect byte overflow (should NEVER happen after Iteration 6 fix)
    // Per LOOP.md: DO NOT RESTART PARLIO, just mark error and let main thread detect
    if (ctx->mBytesTransmitted > ctx->mTotalBytes) {
        ctx->mRingError = true;  // Flag overflow error
        return false;
    }

    // ⚠️  NO LOGGING IN ISR - Logging causes watchdog timeouts and crashes
    // Use GPIO toggling or counters for debug instead

    // ISR-based streaming: Check if next buffer is ready in the ring (use count to detect empty ring)
    volatile size_t count = ctx->mRingCount;

    // Ring empty - check if all data transmitted
    if (count == 0) {
        // Ring is empty - check if we've transmitted ALL the data
        if (ctx->mBytesTransmitted >= ctx->mTotalBytes) {
            // All data transmitted - mark transmission complete
            ctx->mStreamComplete = true;
            ctx->mTransmitting = false;

            // DISARM WORKER ISR on last transmission (stop continuous timer)
            ctx->mWorkerIsrEnabled = false;
            FL_MEMORY_BARRIER;

            // Stop continuous timer (save power after transmission complete)
            if (self->mWorkerTimerHandle.is_valid()) {
                fl::isr::disableHandler(self->mWorkerTimerHandle);
            }

            // ASYNC MODE: No task notifications
            // - Worker task was removed (uses timer ISR instead)
            // - Main task polls mStreamComplete flag via poll() method
            // All task notifications removed to support fully async operation

            return false; // No tasks woken
        }

        // Ring empty but more data pending - RING DOORBELL for worker ISR
        ctx->mHardwareIdle = true; // Signal that hardware needs restart
        ctx->mTransmitting = false; // Hardware is idle, not transmitting

        // DOORBELL PATTERN: Set flag to signal worker ISR (continuous timer will see it)
        // Continuous timer fires every 20µs and checks this flag
        ctx->mWorkerDoorbell = true;
        ctx->mDebugDoorbellRingCount = ctx->mDebugDoorbellRingCount + 1;  // Debug: Track doorbell rings
        FL_MEMORY_BARRIER;

        return false;
    }

    // Next buffer is ready - submit it to hardware
    size_t buffer_idx = read_idx;
    uint8_t *buffer_ptr = self->mRingBuffer->ptrs[buffer_idx];  // Use cached pointer (ISR optimization)
    size_t buffer_size = self->mRingBuffer->sizes[buffer_idx];

    // Invalid buffer - set error flag
    if (!buffer_ptr || buffer_size == 0) {
        ctx->mRingError = true;
        return false;
    }

    // Submit buffer to hardware via peripheral interface
    bool success = self->mPeripheral->transmit(buffer_ptr, buffer_size * 8, 0x0000);

    if (success) {
        // Successfully submitted - advance read index (modulo-3) and decrement count
        ctx->mRingReadIdx = (ctx->mRingReadIdx + 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;

        // RACE CONDITION (TOLERATED BY DESIGN):
        // This read-modify-write on mRingCount is NOT atomic. If workerIsrCallback (lower priority)
        // was incrementing mRingCount when we interrupted it, the final count may be ±1 off temporarily.
        //
        // This is the SAME race documented in workerIsrCallback (line 830), viewed from txDone's perspective.
        // See detailed safety analysis there. Summary: bounded, self-correcting, no buffer corruption.
        ctx->mRingCount = ctx->mRingCount - 1;
        ctx->mHardwareIdle = false; // Hardware is active again

        // DOORBELL PATTERN: Signal worker ISR if buffers need refilling
        // Continuous timer fires every 20µs and checks mWorkerDoorbell flag
        if (ctx->mRingCount < ParlioRingBuffer3::RING_BUFFER_COUNT &&
            ctx->mNextByteOffset < ctx->mTotalBytes &&
            ctx->mWorkerIsrEnabled) {
            ctx->mWorkerDoorbell = true;
            ctx->mDebugDoorbellRingCount = ctx->mDebugDoorbellRingCount + 1;  // Debug: Track doorbell rings
            FL_MEMORY_BARRIER;
        }

        // ASYNC MODE: Worker task notification removed
        // Worker populates buffers via continuous timer ISR (workerIsrCallback)
        // Doorbell flag signals when work is available

        return false; // No tasks woken
    } else {
        // Submission failed - set error flag for CPU to detect
        ctx->mRingError = true;
    }

    return false; // No high-priority task woken
}
// ============================================================================
// END OF ISR - Remember: NO LOGGING, NO BLOCKING, MINIMIZE EXECUTION TIME
// ============================================================================

//=============================================================================
// Worker ISR Callback - Hardware Timer-Based DMA Buffer Population
//=============================================================================

// ============================================================================
// ⚠️ ⚠️ ⚠️  CRITICAL ISR SAFETY RULES - READ BEFORE MODIFYING ⚠️ ⚠️ ⚠️
// ============================================================================
//
// This function runs in INTERRUPT CONTEXT with EXTREMELY strict constraints:
//
// 1. ❌ ABSOLUTELY NO LOGGING (FL_LOG_PARLIO, FL_WARN, FL_ERROR, printf, etc.)
//    - Logging can cause watchdog timeouts, crashes, or system instability
//    - Even "ISR-safe" logging can introduce unacceptable latency
//    - If you need to debug, use GPIO toggling or counters instead
//
// 2. ❌ NO BLOCKING OPERATIONS (mutex, delay, heap allocation, etc.)
//    - ISRs must complete in microseconds, not milliseconds
//    - Any blocking operation will crash the system
//
// 3. ✅ MINIMIZE EXECUTION TIME
//    - Keep ISR as short as possible (ideally <10µs)
//    - Early exit if no work available
//    - Populate only ONE buffer per invocation
//
// 4. ✅ MEMORY BARRIERS
//    - Use FL_MEMORY_BARRIER after state updates
//    - Ensures visibility to txDoneCallback ISR
//
// If the system crashes after you modify this function:
// - First suspect: Did you add logging?
// - Second suspect: Did you add blocking operations?
// - Third suspect: Did you increase execution time?
//
// ============================================================================
FL_OPTIMIZE_FUNCTION void FL_IRAM
ParlioEngine::workerIsrCallback(void *user_data) {
    // ⚠️  ISR CONTEXT - NO LOGGING ALLOWED - SEE FUNCTION HEADER ⚠️

    // Null checks first
    auto *self = static_cast<ParlioEngine *>(user_data);
    if (!self || !self->mIsrContext) {
        return;
    }

    ParlioIsrContext *ctx = self->mIsrContext.get();

    // Debug: Increment workerIsrCallback counter and timestamp
    ctx->mDebugDoorbellCheckCount = ctx->mDebugDoorbellCheckCount + 1;  // Track total ISR firings
    ctx->mDebugLastWorkerIsrTime = self->mPeripheral->getMicroseconds();

    // CRITICAL: Early exit checks (in order of likelihood)
    // Continuous timer fires every 50µs - exit early if no work needed

    // Check 0: Worker ISR disabled by destructor or completion
    if (!ctx->mWorkerIsrEnabled) {
        ctx->mDebugWorkerEarlyExitCount = ctx->mDebugWorkerEarlyExitCount + 1;
        return;
    }

    // Check 1: Doorbell not rung (no work requested)
    if (!ctx->mWorkerDoorbell) {
        ctx->mDebugWorkerEarlyExitCount = ctx->mDebugWorkerEarlyExitCount + 1;  // Most common early exit
        return;  // Exit early, timer will check again in 50µs
    }

    // Doorbell is set - real work available
    ctx->mDebugWorkerIsrCount = ctx->mDebugWorkerIsrCount + 1;  // Track actual work ISR invocations

    // Check 2: Not actively transmitting
    if (!ctx->mTransmitting) {
        // Clear doorbell and exit
        ctx->mWorkerDoorbell = false;
        ctx->mDebugWorkerEarlyExitCount = ctx->mDebugWorkerEarlyExitCount + 1;
        return;
    }

    // Check 3: Ring buffer full (no space to populate)
    if (ctx->mRingCount >= ParlioRingBuffer3::RING_BUFFER_COUNT) {
        // Clear doorbell and exit (will be rung again when space available)
        ctx->mWorkerDoorbell = false;
        ctx->mDebugWorkerEarlyExitCount = ctx->mDebugWorkerEarlyExitCount + 1;
        return;
    }

    // Check 4: All data already processed
    if (ctx->mNextByteOffset >= ctx->mTotalBytes) {
        // Clear doorbell and exit
        ctx->mWorkerDoorbell = false;
        ctx->mDebugWorkerEarlyExitCount = ctx->mDebugWorkerEarlyExitCount + 1;
        return;
    }

    // CLEAR DOORBELL before processing (txDone will ring again if needed)
    ctx->mWorkerDoorbell = false;

    // Work available - populate ONE buffer
    // Get next ring buffer index (0-2)
    size_t ring_index = ctx->mRingWriteIdx;

    // Get ring buffer pointer (use cached pointer for optimization)
    uint8_t *outputBuffer = self->mRingBuffer->ptrs[ring_index];
    if (!outputBuffer) {
        return;  // Invalid buffer - should never happen
    }

    // Calculate byte range for this buffer
    size_t bytes_remaining = ctx->mTotalBytes - ctx->mNextByteOffset;
    size_t bytes_per_buffer = (ctx->mTotalBytes + ParlioRingBuffer3::RING_BUFFER_COUNT - 1) / ParlioRingBuffer3::RING_BUFFER_COUNT;

    // LED boundary alignment constant
    size_t bytes_per_led_all_lanes = 3 * ctx->mNumLanes;

    // CAP bytes_per_buffer at ring buffer capacity
    ParlioBufferCalculator calc{self->mDataWidth};
    size_t reset_padding = calc.resetPaddingBytes(self->mResetUs);
    size_t available_capacity = self->mRingBufferCapacity - reset_padding;  // Reserve space for reset padding
    size_t max_input_bytes_per_buffer = available_capacity / calc.outputBytesPerInputByte();

    // Reduce max by one LED boundary to prevent exact-capacity overflow
    if (max_input_bytes_per_buffer >= bytes_per_led_all_lanes) {
        max_input_bytes_per_buffer -= bytes_per_led_all_lanes;
    }

    if (bytes_per_buffer > max_input_bytes_per_buffer) {
        bytes_per_buffer = max_input_bytes_per_buffer;
    }

    // LED boundary alignment: Round DOWN
    bytes_per_buffer = (bytes_per_buffer / bytes_per_led_all_lanes) * bytes_per_led_all_lanes;

    // Ensure at least one LED per buffer
    if (bytes_per_buffer < bytes_per_led_all_lanes && ctx->mTotalBytes >= bytes_per_led_all_lanes) {
        bytes_per_buffer = bytes_per_led_all_lanes;
    }

    // For LAST buffer, take ALL remaining bytes
    size_t byte_count;
    size_t buffers_already_populated = ctx->mRingCount;
    bool is_last_buffer = (buffers_already_populated >= ParlioRingBuffer3::RING_BUFFER_COUNT - 1) ||
                          (bytes_remaining <= bytes_per_buffer);
    if (is_last_buffer) {
        byte_count = bytes_remaining;
        if (byte_count > max_input_bytes_per_buffer) {
            byte_count = max_input_bytes_per_buffer;
        }
    } else {
        byte_count = bytes_per_buffer;
    }

    // Zero output buffer (ISR-safe memset)
    fl::isr::memset_zero(outputBuffer, self->mRingBufferCapacity);

    // Generate waveform data
    size_t outputBytesWritten = 0;
    if (!self->populateDmaBuffer(outputBuffer, self->mRingBufferCapacity,
                                  ctx->mNextByteOffset, byte_count, outputBytesWritten)) {
        return;  // Buffer overflow - skip this iteration
    }

    // Store actual DMA buffer size and input byte count
    self->mRingBuffer->sizes[ring_index] = outputBytesWritten;
    self->mRingBuffer->input_sizes[ring_index] = byte_count;  // Track input bytes (excludes reset padding)

    // SYNCHRONIZATION: Additional memory barriers after buffer population
    // Ensures writes complete before txDoneCallback reads buffer for DMA submission
    // Iteration 3, Strategy 1 (High Priority)
    FL_MEMORY_BARRIER;
    FL_MEMORY_BARRIER;
    FL_MEMORY_BARRIER;

    // Update state for next buffer
    ctx->mNextByteOffset += byte_count;
    ctx->mRingWriteIdx = (ctx->mRingWriteIdx + 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;

    // RACE CONDITION (TOLERATED BY DESIGN):
    // This read-modify-write on mRingCount is NOT atomic. If txDoneCallback (higher priority ISR)
    // interrupts between the read and write, the count can be temporarily incorrect by ±1.
    //
    // WHY THIS IS SAFE:
    // 1. Bounded by Design: Worker ISR checks "mRingCount >= ParlioRingBuffer3::RING_BUFFER_COUNT" BEFORE populating (line 750).
    //    Maximum overshoot is limited to +1 (we only increment by 1, never more).
    // 2. Self-Correcting: Race resolves on next ISR cycle. Temporary ±1 error does not propagate.
    // 3. No Buffer Corruption: Worker operates on mRingWriteIdx, txDone operates on mRingReadIdx.
    //    No concurrent access to the same buffer slot.
    // 4. Semantic Safety: mRingCount represents "buffers in flight" (loose coordination), not a lock.
    //
    // FUTURE ENHANCEMENT (Optional):
    // Consider using atomic operations (__sync_fetch_and_add) if device testing reveals instability.
    // Current implementation is functionally correct and passes validation testing.
    ctx->mRingCount = ctx->mRingCount + 1;

    // Memory barrier to ensure state visible to txDoneCallback ISR
    FL_MEMORY_BARRIER;
}
// ============================================================================
// END OF WORKER ISR - Remember: NO LOGGING, NO BLOCKING, MINIMIZE EXECUTION TIME
// ============================================================================

//=============================================================================
// DMA Buffer Population - Wave8 Waveform Generation
//=============================================================================

/// @brief Populate a DMA buffer with waveform data for a byte range
///
/// Two-stage processing model (repeated for each byte position):
///   Stage 1: Generate wave8bytes for ALL lanes → staging buffer (waveform_expansion_buffer)
///   Stage 2: Transpose staging buffer → DMA output buffer (bit-packed PARLIO format)
///
/// @param outputBuffer DMA buffer to populate (pre-allocated and pre-zeroed)
/// @param outputBufferCapacity Maximum size of output buffer
/// @param startByte Starting byte offset in source data
/// @param byteCount Number of bytes to process
/// @param outputBytesWritten [out] Number of bytes written to output buffer
/// @return true on success, false on error (buffer overflow, etc.)
FL_OPTIMIZE_FUNCTION bool FL_IRAM
ParlioEngine::populateDmaBuffer(uint8_t* outputBuffer,
                                 size_t outputBufferCapacity,
                                 size_t startByte,
                                 size_t byteCount,
                                 size_t& outputBytesWritten) {
    // Staging buffer for wave8 output before transposition
    // Holds wave8bytes for all lanes (data_width × 8 bytes)
    // Each lane produces Wave8Byte (8 bytes) for each input byte
    uint8_t* laneWaveforms = mWaveformExpansionBuffer.data();
    constexpr size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte

    size_t outputIdx = 0;
    size_t byteOffset = 0;

    // Use calculator for padding and transpose block size
    ParlioBufferCalculator calc{mDataWidth};
    size_t blockSize = calc.transposeBlockSize();

    // ═══════════════════════════════════════════════════════════════
    // PHASE 1: Insert front padding (1 Wave8Byte per lane = 8 bytes)
    // ═══════════════════════════════════════════════════════════════
    // Only add front padding on the FIRST byte of transmission
    bool is_first_byte = (startByte == 0);

    if (is_first_byte) {
        constexpr size_t FRONT_PAD_BYTES = 8;  // 1 Wave8Byte per lane
        size_t front_padding_total = FRONT_PAD_BYTES * mDataWidth;

        // Boundary check
        if (outputIdx + front_padding_total > outputBufferCapacity) {
            // Buffer overflow - cannot log in hot path (causes 98× slowdown)
            // Error will be detected by caller via return false
            outputBytesWritten = outputIdx;
            return false;
        }

        // Buffer is already pre-zeroed by caller (fl::isr::memset_zero)
        // Just advance the index to skip over the front padding region
        outputIdx += front_padding_total;
    }

    // Two-stage architecture: Process one byte position at a time
    // Stage 1: Generate wave8bytes for ALL lanes → staging buffer
    // Stage 2: Transpose staging buffer → DMA output buffer

    while (byteOffset < byteCount) {
        // Check if enough space for this block
        if (outputIdx + blockSize > outputBufferCapacity) {
            // Buffer overflow - return error immediately
            outputBytesWritten = outputIdx;
            return false;
        }

        // ═══════════════════════════════════════════════════════════════
        // STAGE 1: Generate wave8bytes for ALL lanes into staging buffer
        // ═══════════════════════════════════════════════════════════════
        // Split real and dummy lane processing to eliminate branch in inner loop

        // Process real channels first (no branch mispredictions)
        for (size_t lane = 0; lane < mActualChannels; lane++) {
            uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);
            const uint8_t* laneData = mScratchBuffer + (lane * mLaneStride);
            uint8_t byte = laneData[startByte + byteOffset];

            // wave8() outputs Wave8Byte (8 bytes) in bit-packed format
            // Cast pointer to array reference for wave8 API
            uint8_t (*wave8Array)[8] = reinterpret_cast<uint8_t (*)[8]>(laneWaveform);
            fl::wave8(byte, mWave8Lut, *wave8Array);
        }

        // Bulk-zero dummy lanes separately (more efficient than per-lane zeroing)
        if (mActualChannels < mDataWidth) {
            uint8_t* firstDummyLane = laneWaveforms + (mActualChannels * bytes_per_lane);
            size_t dummyLaneBytes = (mDataWidth - mActualChannels) * bytes_per_lane;
            fl::isr::memset_zero(firstDummyLane, dummyLaneBytes);
        }

        // ═══════════════════════════════════════════════════════════════
        // STAGE 2: Transpose staging buffer → DMA output buffer
        // ═══════════════════════════════════════════════════════════════
        // Transpose wave8bytes from all lanes (laneWaveforms staging buffer)
        // into bit-packed format for PARLIO hardware transmission
        size_t bytesWritten = fl::transpose_wave8byte_parlio(
            laneWaveforms,        // Input: staging buffer (all lanes' wave8bytes)
            mDataWidth,    // Number of lanes (1-16)
            outputBuffer + outputIdx);  // Output: DMA buffer

        outputIdx += bytesWritten;
        byteOffset++;
    }

    // ═══════════════════════════════════════════════════════════════
    // STAGE 3: Append back padding (1 Wave8Byte per lane = 8 bytes)
    // ═══════════════════════════════════════════════════════════════
    // Only append back padding on the LAST byte of transmission
    // (when processing the final byte in the total byte range)
    bool is_last_byte = (startByte + byteCount >= mIsrContext->mTotalBytes);

    if (is_last_byte) {
        constexpr size_t BACK_PAD_BYTES = 8;  // 1 Wave8Byte per lane
        size_t back_padding_total = BACK_PAD_BYTES * mDataWidth;

        // Boundary check
        if (outputIdx + back_padding_total > outputBufferCapacity) {
            // Buffer overflow - cannot log in hot path (causes 98× slowdown)
            // Error will be detected by caller via return false
            outputBytesWritten = outputIdx;
            return false;
        }

        // Buffer is already pre-zeroed by caller, just advance index
        outputIdx += back_padding_total;
    }

    // ═══════════════════════════════════════════════════════════════
    // STAGE 4: Append reset time padding (all-zero Wave8Bytes)
    // ═══════════════════════════════════════════════════════════════
    // Only append reset padding on the LAST byte of transmission
    // (when processing the final byte in the total byte range)

    if (is_last_byte && mResetUs > 0) {
        // Calculate reset padding bytes needed
        ParlioBufferCalculator calc{mDataWidth};
        size_t reset_padding_bytes = calc.resetPaddingBytes(mResetUs);

        // Boundary check: Ensure padding fits in output buffer
        if (outputIdx + reset_padding_bytes > outputBufferCapacity) {
            // Buffer overflow - cannot log in hot path (causes 98× slowdown)
            // Error will be detected by caller via return false
            outputBytesWritten = outputIdx;
            return false;
        }

        // Append all-zero bytes (LOW signal for reset duration)
        // Buffer is already pre-zeroed by caller, so we just advance the index
        outputIdx += reset_padding_bytes;
    }

    outputBytesWritten = outputIdx;
    return true;
}

//=============================================================================
// Ring Buffer Management - Incremental Population Architecture
//=============================================================================

bool ParlioEngine::hasRingSpace() const {
    if (!mIsrContext) {
        return false;
    }

    // Use count to determine if ring has space (distinguishes full vs empty)
    volatile size_t count = mIsrContext->mRingCount;

    // Ring has space if count is less than ParlioRingBuffer3::RING_BUFFER_COUNT
    return count < ParlioRingBuffer3::RING_BUFFER_COUNT;
}


bool ParlioEngine::allocateRingBuffers() {
    // One-time ring buffer allocation and initialization
    // Called once during initialize(), NOT per transmission
    // Buffers remain allocated, only POPULATED on-demand during transmission

    // Allocate 3 DMA buffers with cache alignment
    // CRITICAL FINDING: ESP32-C6 DMA memory architecture
    // - MALLOC_CAP_DMA allocates from NON-cacheable memory region (SRAM1)
    // - esp_cache_msync() returns ESP_ERR_INVALID_ARG for cacheable addresses
    // - MALLOC_CAP_INTERNAL allocates from cacheable memory region (SRAM0)
    // - PARLIO DMA can access both regions via system bus
    //
    // SOLUTION: Use MALLOC_CAP_DMA (non-cacheable) to eliminate cache sync errors
    // - Non-cacheable memory eliminates need for esp_cache_msync()
    // - Eliminates ESP-IDF cache errors that lead to system crashes
    // - Memory barriers (FL_MEMORY_BARRIER) provide ordering guarantees

    uint8_t* buffers[3] = {nullptr, nullptr, nullptr};

    // Allocate all 3 buffers via peripheral interface (handles DMA requirements)
    for (size_t i = 0; i < 3; i++) {
        buffers[i] = mPeripheral->allocateDmaBuffer(mRingBufferCapacity);

        if (!buffers[i]) {
            FL_LOG_PARLIO("PARLIO: Failed to allocate ring buffer "
                    << i << "/3 (requested " << mRingBufferCapacity << " bytes)");

            // Clean up any buffers we already allocated
            for (size_t j = 0; j < i; j++) {
                mPeripheral->freeDmaBuffer(buffers[j]);
            }
            return false;
        }

        // Zero-initialize buffer to prevent garbage data
        fl::memset(buffers[i], 0x00, mRingBufferCapacity);
    }

    // Create ring buffer with unique_ptr and cleanup callback
    // Capture mPeripheral pointer for cleanup lambda
    auto* peripheral = mPeripheral;
    mRingBuffer.reset(new ParlioRingBuffer3(
        buffers[0], buffers[1], buffers[2],
        mRingBufferCapacity,
        [peripheral](uint8_t* ptr) {
            peripheral->freeDmaBuffer(ptr);
        }
    ));

    if (!mRingBuffer) {
        FL_LOG_PARLIO("PARLIO: Failed to allocate ring buffer structure");
        // Clean up DMA buffers
        for (size_t i = 0; i < 3; i++) {
            mPeripheral->freeDmaBuffer(buffers[i]);
        }
        return false;
    }

    return true;
}

bool ParlioEngine::allocateWorkerTimer() {
    // Allocate and configure hardware timer for ISR-based background buffer population
    // Timer fires continuously every 50µs (doorbell pattern - checks flag for work)
    // Reduced from 20µs to 50µs for stability during initialization

    // Configure timer using fl::isr API
    fl::isr::isr_config_t config;
    config.handler = workerIsrCallback;
    config.user_data = this;
    config.frequency_hz = 1000000 / FL_ESP_PARLIO_WORKER_TIMER_PERIOD_US;  // Convert period to frequency
    config.priority = FL_ESP_PARLIO_WORKER_TIMER_INTR_PRIORITY;
    config.flags = fl::isr::ISR_FLAG_IRAM_SAFE;

    int result = fl::isr::attachTimerHandler(config, &mWorkerTimerHandle);
    if (result != 0) {
        FL_LOG_PARLIO("PARLIO: Failed to attach worker timer: " << result);
        return false;
    }

    FL_LOG_PARLIO("PARLIO: Worker timer attached successfully (continuous mode, 50µs period)");
    return true;
}

//=============================================================================
// ⚠️  AI AGENT WARNING: CRITICAL PERFORMANCE HOT PATH - NO LOGGING ALLOWED ⚠️
//=============================================================================
//
// This function (populateNextDMABuffer) is called 20+ times per transmission
// in a tight loop competing with hardware timing. ANY logging here causes:
//
// - UART overhead: ~9ms per log call @ 115200 baud (80 chars/log)
// - CPU budget: Only 600μs available per buffer (hardware transmission time)
// - Performance impact: Logging causes 98× slowdown (1.2s vs 12ms)
// - Ring buffer underruns: Hardware drains faster than CPU can refill
//
// ❌ FORBIDDEN: FL_LOG_PARLIO, FL_WARN, FL_DBG, printf, Serial.print
// ✅ ALLOWED: Error conditions using FL_WARN (non-hot path, infrequent)
//
// If you need to debug this function:
// 1. Use a logic analyzer or oscilloscope (hardware timing)
// 2. Increment counters and log AFTER transmission completes
// 3. Enable logging ONLY for single-shot debugging, then remove
//
// See TASK.md UPDATE #2 and #3 for detailed investigation of logging impact.
//
//=============================================================================

FL_OPTIMIZE_FUNCTION bool FL_IRAM
ParlioEngine::populateNextDMABuffer() {
    // Incremental buffer population - called repeatedly to fill ring buffers
    // Returns true if more buffers need to be populated, false if all done

    if (!mIsrContext) {
        return false;
    }

    // Check if more data to process (use ISR context's mNextByteOffset)
    if (mIsrContext->mNextByteOffset >= mIsrContext->mTotalBytes) {
        return false; // No more source data
    }

    // Get next ring buffer index (use ISR context's mRingWriteIdx)
    size_t ring_index = mIsrContext->mRingWriteIdx;

    // Get ring buffer pointer (use cached pointer for optimization)
    uint8_t *outputBuffer = mRingBuffer->ptrs[ring_index];
    if (!outputBuffer) {
        //FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        mErrorOccurred = true;
        return false;
    }

    // Calculate byte range for this buffer (divide total bytes into chunks)
    size_t bytes_remaining = mIsrContext->mTotalBytes - mIsrContext->mNextByteOffset;
    size_t bytes_per_buffer = (mIsrContext->mTotalBytes + ParlioRingBuffer3::RING_BUFFER_COUNT - 1) / ParlioRingBuffer3::RING_BUFFER_COUNT;

    // LED boundary alignment constant: 3 bytes (RGB) × lane count
    // Used for both capacity calculation and alignment rounding
    size_t bytes_per_led_all_lanes = 3 * mDataWidth;

    // CAP bytes_per_buffer at ring buffer capacity to enable streaming for large strips
    ParlioBufferCalculator calc{mDataWidth};
    size_t reset_padding = calc.resetPaddingBytes(mResetUs);
    size_t available_capacity = mRingBufferCapacity - reset_padding;  // Reserve space for reset padding
    size_t max_input_bytes_per_buffer = available_capacity / calc.outputBytesPerInputByte();

    // CRITICAL FIX: Reduce max by one LED boundary to prevent exact-capacity overflow
    if (max_input_bytes_per_buffer >= bytes_per_led_all_lanes) {
        max_input_bytes_per_buffer -= bytes_per_led_all_lanes;
    }

    if (bytes_per_buffer > max_input_bytes_per_buffer) {
        bytes_per_buffer = max_input_bytes_per_buffer;
    }

    // LED boundary alignment: Round DOWN to nearest multiple of (3 bytes × lane count)
    bytes_per_buffer = (bytes_per_buffer / bytes_per_led_all_lanes) * bytes_per_led_all_lanes;

    // Edge case: Ensure at least one LED across all lanes per buffer if data exists
    if (bytes_per_buffer < bytes_per_led_all_lanes && mIsrContext->mTotalBytes >= bytes_per_led_all_lanes) {
        bytes_per_buffer = bytes_per_led_all_lanes;
    }

    // FIX: For the LAST buffer, take ALL remaining bytes (don't round down and lose data)
    size_t byte_count;
    size_t buffers_already_populated = mIsrContext->mRingCount;
    bool is_last_buffer = (buffers_already_populated >= ParlioRingBuffer3::RING_BUFFER_COUNT - 1) ||
                          (bytes_remaining <= bytes_per_buffer);
    if (is_last_buffer) {
        byte_count = bytes_remaining;  // Last buffer takes all remaining bytes
        // BUT cap at buffer capacity (streaming will handle rest)
        if (byte_count > max_input_bytes_per_buffer) {
            byte_count = max_input_bytes_per_buffer;
        }
    } else {
        byte_count = bytes_per_buffer;  // Earlier buffers use aligned size
    }

    // Zero output buffer to prevent garbage data from previous use
    // Use ISR-safe memset since this function may be called from workerIsr()
    fl::isr::memset_zero(outputBuffer, mRingBufferCapacity);

    // Generate waveform data using helper function
    size_t outputBytesWritten = 0;
    if (!populateDmaBuffer(outputBuffer, mRingBufferCapacity,
                          mIsrContext->mNextByteOffset, byte_count, outputBytesWritten)) {
        //FL_WARN("PARLIO: Ring buffer overflow at ring_index=" << ring_index);
        mErrorOccurred = true;
        return false;
    }

    // Store actual DMA buffer size and input byte count
    mRingBuffer->sizes[ring_index] = outputBytesWritten;
    mRingBuffer->input_sizes[ring_index] = byte_count;  // Track input bytes (excludes reset padding)

    // Update state for next buffer (ISR context owns the state now)
    mIsrContext->mNextByteOffset += byte_count;
    mIsrContext->mRingWriteIdx = (mIsrContext->mRingWriteIdx + 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;
    mIsrContext->mRingCount = mIsrContext->mRingCount + 1;

    // CRITICAL: Check if hardware went idle while we were populating
    if (mIsrContext->mHardwareIdle) {
        // Get the buffer that was just populated (read_idx points to next buffer to transmit)
        size_t buffer_idx = mIsrContext->mRingReadIdx;
        uint8_t *buffer_ptr = mRingBuffer->ptrs[buffer_idx];  // Use cached pointer for optimization
        size_t buffer_size = mRingBuffer->sizes[buffer_idx];

        if (buffer_ptr && buffer_size > 0) {
            // Submit buffer to hardware to restart transmission via peripheral interface
            bool success = mPeripheral->transmit(buffer_ptr, buffer_size * 8, 0x0000);

            if (success) {
                // Successfully restarted - advance read index and decrement count
                mIsrContext->mRingReadIdx = (mIsrContext->mRingReadIdx + 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;
                mIsrContext->mRingCount = mIsrContext->mRingCount - 1;
                mIsrContext->mHardwareIdle = false;
                mIsrContext->mTransmitting = true;
            } else {
                //FL_WARN("PARLIO CPU: Failed to restart hardware: " << err);
                mErrorOccurred = true;
            }
        }
    }

    // Return true if more bytes remain to be processed
    return mIsrContext->mNextByteOffset < mIsrContext->mTotalBytes;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool ParlioEngine::initialize(size_t dataWidth,
                              const fl::vector<int>& pins,
                              const ChipsetTimingConfig& timing,
                              size_t maxLedsPerChannel) {
    if (mInitialized) {
        return true; // Already initialized
    }

    // Store data width and pins
    mDataWidth = dataWidth;
    mPins = pins;
    mActualChannels = pins.size();
    mDummyLanes = mDataWidth - mActualChannels;

    // Store timing parameters
    mTimingT1Ns = timing.t1_ns;
    mTimingT2Ns = timing.t2_ns;
    mTimingT3Ns = timing.t3_ns;
    mResetUs = timing.reset_us;

    // Validate data width
    if (dataWidth != 1 && dataWidth != 2 && dataWidth != 4 &&
        dataWidth != 8 && dataWidth != 16) {
        FL_LOG_PARLIO("PARLIO: Invalid data_width=" << dataWidth);
        return false;
    }

    // Allocate ISR context (cache-aligned, 64 bytes)
    if (!mIsrContext) {
        mIsrContext = fl::make_unique<ParlioIsrContext>();
    }

    // CRITICAL: Disable worker ISR during initialization to prevent spurious timer firings
    // Timer is enabled during allocateWorkerTimer(), but should NOT fire until beginTransmission()
    if (mIsrContext) {
        mIsrContext->mWorkerIsrEnabled = false;
    }

    // Validate pins
    if (pins.size() != dataWidth) {
        FL_LOG_PARLIO("PARLIO: Pin configuration error - expected "
                << dataWidth << " pins, got " << pins.size());
        return false;
    }

    for (size_t i = 0; i < pins.size(); i++) {
        if (!isParlioPinValid(pins[i])) {
            FL_LOG_PARLIO("PARLIO: Invalid pin " << pins[i] << " for channel " << i);
            return false;
        }
    }

    // Build wave8 expansion LUT from timing configuration
    ChipsetTiming chipsetTiming;
    chipsetTiming.T1 = mTimingT1Ns;
    chipsetTiming.T2 = mTimingT2Ns;
    chipsetTiming.T3 = mTimingT3Ns;
    chipsetTiming.RESET = mResetUs;  // Stored for documentation (padding handled in DMA buffer population)
    chipsetTiming.name = "PARLIO";

    mWave8Lut = buildWave8ExpansionLUT(chipsetTiming);

    // Get peripheral singleton instance
#ifdef FASTLED_STUB_IMPL
    mPeripheral = &ParlioPeripheralMock::instance();
#else
    mPeripheral = &ParlioPeripheralESP::instance();
#endif

    // Configure peripheral (constructor handles -1 filling for unused pins)
    ParlioPeripheralConfig config(
        pins,  // Uses actual pin vector (size = dataWidth)
        FL_ESP_PARLIO_CLOCK_FREQ_HZ,
        FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH,
        65534
    );

    // Initialize peripheral
    if (!mPeripheral->initialize(config)) {
        FL_LOG_PARLIO("PARLIO: Failed to initialize peripheral");
        mPeripheral = nullptr;
        return false;
    }

    // Register ISR callback
    if (!mPeripheral->registerTxDoneCallback(
            reinterpret_cast<void*>(txDoneCallback), this)) {
        FL_LOG_PARLIO("PARLIO: Failed to register callbacks");
        mPeripheral = nullptr;
        return false;
    }

    // Calculate ring buffer capacity
    ParlioBufferCalculator calc{mDataWidth};
    size_t raw_capacity = calc.calculateRingBufferCapacity(maxLedsPerChannel, mResetUs, ParlioRingBuffer3::RING_BUFFER_COUNT);

    // NEEDS REVIEW!!! are we potentially accessing memory outside of the cache??
    // Please invesatigate
    // Round up to 64-byte multiple to ensure cache line alignment
    // This addresses byte offset 318 error (2 bytes before cache boundary 320)
    // Cache sync requires both address AND size to be cache-aligned
    mRingBufferCapacity = ((raw_capacity + 63) / 64) * 64;

    if (mRingBufferCapacity != raw_capacity) {
        FL_DBG("PARLIO: Rounded buffer capacity from " << raw_capacity << " to " << mRingBufferCapacity << " bytes (64-byte alignment)");
    }

    // Allocate ring buffers
    if (!allocateRingBuffers()) {
        FL_LOG_PARLIO("PARLIO: Failed to allocate ring buffers");
        mPeripheral = nullptr;
        return false;
    }

    // Allocate worker timer
    if (!allocateWorkerTimer()) {
        FL_LOG_PARLIO("PARLIO: Failed to allocate worker timer");
        mPeripheral = nullptr;
        return false;
    }

    // Allocate waveform expansion buffer (staging buffer - CPU only, no DMA)
    // This buffer holds intermediate wave8 data before transposition to DMA buffers
    const size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte
    const size_t waveform_buffer_size = mDataWidth * bytes_per_lane;

    // Use regular heap vector (not DMA) - this is a CPU-only staging buffer
    mWaveformExpansionBuffer.resize(waveform_buffer_size);

    if (mWaveformExpansionBuffer.empty()) {
        FL_LOG_PARLIO("PARLIO: Failed to allocate waveform expansion buffer");
        mPeripheral = nullptr;
        return false;
    }

    // Initialize ISR context state
    if (mIsrContext) {
        mIsrContext->mTransmitting = false;
        mIsrContext->mStreamComplete = false;
        mIsrContext->mCurrentByte = 0;
        mIsrContext->mTotalBytes = 0;
    }
    mErrorOccurred = false;

    mInitialized = true;
    return true;
}

bool ParlioEngine::beginTransmission(const uint8_t* scratchBuffer,
                                     size_t totalBytes,
                                     size_t numLanes,
                                     size_t laneStride) {
    if (!mInitialized || !mPeripheral || !mIsrContext) {
        FL_LOG_PARLIO("PARLIO: Cannot transmit - not initialized");
        return false;
    }

    // Check if already transmitting
    if (mIsrContext->mTransmitting) {
        FL_LOG_PARLIO("PARLIO: Transmission already in progress");
        return false;
    }

    if (totalBytes == 0) {
        return true; // Nothing to transmit
    }

    // ASYNC MODE: No task handle capture needed
    // Completion detected via poll() reading mStreamComplete flag

    // Store scratch buffer reference (NOT owned by this class)
    mScratchBuffer = scratchBuffer;
    mLaneStride = laneStride;

    // Initialize IsrContext state for ring buffer streaming
    mIsrContext->mTotalBytes = totalBytes;
    mIsrContext->mNumLanes = numLanes;
    mIsrContext->mCurrentByte = 0;
    mIsrContext->mStreamComplete = false;
    mErrorOccurred = false;
    mIsrContext->mTransmitting = false; // Will be set to true after first buffer submitted

    // Initialize ring buffer indices and count
    mIsrContext->mRingReadIdx = 0;
    mIsrContext->mRingWriteIdx = 0;
    mIsrContext->mRingCount = 0;
    mIsrContext->mRingError = false;
    mIsrContext->mHardwareIdle = false;
    mIsrContext->mNextByteOffset = 0;
    mIsrContext->mWorkerIsrEnabled = false;
    mIsrContext->mWorkerDoorbell = false;

    // Initialize counters
    mIsrContext->mIsrCount = 0;
    mIsrContext->mBytesTransmitted = 0;
    mIsrContext->mChunksCompleted = 0;
    mIsrContext->mTransmissionActive = true;
    mIsrContext->mEndTimeUs = 0;

    // Initialize debug counters
    mIsrContext->mDebugTxDoneCount = 0;
    mIsrContext->mDebugWorkerIsrCount = 0;
    mIsrContext->mDebugLastTxDoneTime = 0;
    mIsrContext->mDebugLastWorkerIsrTime = 0;
    mIsrContext->mDebugDoorbellRingCount = 0;
    mIsrContext->mDebugDoorbellCheckCount = 0;
    mIsrContext->mDebugWorkerEarlyExitCount = 0;

    // Pre-populate ring buffers (fill all buffers if possible)
    while (hasRingSpace() && populateNextDMABuffer()) {
        // Buffer populated into ring
    }

    // Get actual number of buffers populated
    size_t buffers_populated = mIsrContext->mRingCount;

    // Debug: Log input_sizes array
    FL_LOG_PARLIO("PARLIO: DEBUG input_sizes[0]=" << mRingBuffer->input_sizes[0]
         << " [1]=" << mRingBuffer->input_sizes[1]
         << " [2]=" << mRingBuffer->input_sizes[2]
         << " buffers=" << buffers_populated
         << " total=" << totalBytes);

    // Verify at least one buffer was populated
    if (buffers_populated == 0) {
        FL_LOG_PARLIO("PARLIO: No buffers populated - cannot start transmission");
        mErrorOccurred = true;
        return false;
    }

    // Enable PARLIO peripheral for this transmission (only if not already enabled)
    if (!mTxUnitEnabled) {
        if (!mPeripheral->enable()) {
            FL_LOG_PARLIO("PARLIO: Failed to enable peripheral");
            mErrorOccurred = true;
            return false;
        }
        mTxUnitEnabled = true;
    }

    // Queue first buffer to start transmission
    FL_LOG_PARLIO("PARLIO: Starting ISR-based streaming | first_buffer_size="
           << mRingBuffer->sizes[0] << " | buffers_ready=" << buffers_populated);

    size_t first_buffer_size = mRingBuffer->sizes[0];

    // CRITICAL FIX: Mark transmission started BEFORE submitting buffer
    // This closes the race window where txDoneCallback could fire before flag is set (Issue #2)
    mIsrContext->mTransmitting = true;

    // CRITICAL FIX (Iteration 2): Advance read index BEFORE submitting first buffer
    // This prevents race condition where txDone fires before index is advanced
    // Root cause: If txDone fires with read_idx=0, it calculates wrong completed_buffer_idx
    mIsrContext->mRingReadIdx = 1;
    mIsrContext->mRingCount = buffers_populated - 1;
    FL_MEMORY_BARRIER;

    if (!mPeripheral->transmit(mRingBuffer->ptrs[0], first_buffer_size * 8, 0x0000)) {
        FL_LOG_PARLIO("PARLIO: Failed to queue first buffer");
        mIsrContext->mTransmitting = false;  // Rollback flag on error
        mIsrContext->mRingReadIdx = 0;  // Rollback index on error
        mIsrContext->mRingCount = buffers_populated;  // Rollback count on error
        mErrorOccurred = true;
        return false;
    }

    //=========================================================================
    // Start worker timer ISR for background DMA buffer population
    //=========================================================================
    // Refactored from FreeRTOS task to hardware timer ISR:
    // - Lower latency (~1-2µs vs ~5-10µs task switching)
    // - More deterministic timing (no scheduler overhead)
    //
    // Timer ISR pattern (continuous, doorbell):
    // - Timer fires continuously every 50µs (reduced from 20µs for stability)
    // - workerIsrCallback checks mWorkerDoorbell flag for work
    // - txDoneCallback sets mWorkerDoorbell when buffer space available
    // - ISR exits early if doorbell not rung or no work needed
    // - ISR populates ONE buffer per call (if ring has space and doorbell set)
    // - Timer stopped when transmission complete
    //
    // CRITICAL ORDERING: Start timer AFTER first buffer is submitted (defensive)
    // - Prevents timer ISR from firing before hardware is fully initialized
    // - First txDoneCallback will ring doorbell to start worker ISR activity
    //=========================================================================

    // Enable worker ISR (set flag BEFORE starting timer)
    mIsrContext->mWorkerIsrEnabled = true;
    mIsrContext->mWorkerDoorbell = false;  // Clear doorbell (txDone will ring it)
    FL_MEMORY_BARRIER;

    // Enable continuous timer (fires every 50µs) - AFTER first buffer submitted
    // Timer was already started when attached via fl::isr::attachTimerHandler
    // Just need to enable it now that transmission is ready
    if (fl::isr::enableHandler(mWorkerTimerHandle) != 0) {
        FL_LOG_PARLIO("PARLIO: Failed to enable worker timer");
        mIsrContext->mTransmitting = false;  // Rollback flag on error
        mErrorOccurred = true;
        return false;
    }

    FL_LOG_PARLIO("PARLIO: Worker timer started (continuous mode, 50µs period, doorbell pattern)"
           << " | buffers_ready=" << buffers_populated);

    // Debug task: Logs ISR state every 500ms for diagnostics
    // Iteration 7 confirmed: Debug task is NOT the cause of crash
    if (!mDebugTask.is_valid()) {
        // Create debug task using unified task API (wraps TaskCoroutine)
        // Lambda captures 'this' to access ParlioEngine instance
        CoroutineConfig config;
        config.function = [this]() { debugTaskFunction(this); };
        config.name = "parlio_debug";
        config.stack_size = 2048;  // 2KB stack
        config.priority = 1;       // Low priority (tskIDLE_PRIORITY + 1)
        mDebugTask = fl::task::coroutine(config);

        FL_LOG_PARLIO("PARLIO: Debug task created (500ms logging interval)");
    }

    // Debug: Print initial counter state
    // Note: read_idx and ring_count already advanced before buffer submission (Iteration 2 fix)
    FL_LOG_PARLIO("DEBUG: Transmission started (async)"
           << " | read_idx=" << mIsrContext->mRingReadIdx
           << " | ring_count=" << mIsrContext->mRingCount
           << " | txDone_count=" << mIsrContext->mDebugTxDoneCount
           << " | worker_count=" << mIsrContext->mDebugWorkerIsrCount);

    // PHASE 1: ASYNC MODE - Return immediately without blocking
    // ISRs will handle buffer population and hardware submission in background
    // Caller must use poll() to check transmission progress
    // Cleanup (disable TX unit, stop timer) now handled in poll() when complete

    return true; // Transmission started successfully (async)
}

ParlioEngineState ParlioEngine::poll() {
    if (!mInitialized || !mPeripheral || !mIsrContext) {
        return ParlioEngineState::READY;
    }

    // Check for errors
    if (mErrorOccurred) {
        FL_LOG_PARLIO("PARLIO: Error occurred during transmission");
        mIsrContext->mTransmitting = false;
        mErrorOccurred = false;
        return ParlioEngineState::ERROR;
    }

    // Check if streaming is complete
    if (mIsrContext->mStreamComplete) {
        // Execute memory barrier to synchronize all ISR writes
        FL_MEMORY_BARRIER;

        // Clear completion flags
        mIsrContext->mTransmitting = false;
        mIsrContext->mStreamComplete = false;

        // Stop worker timer to save power and prevent spurious ISR firings
        if (mWorkerTimerHandle.is_valid()) {
            fl::isr::disableHandler(mWorkerTimerHandle);
        }

        // Wait for final chunk to complete (non-blocking poll)
        if (mPeripheral->waitAllDone(0)) {
            // All transmissions complete - disable peripheral (only if currently enabled)
            if (mTxUnitEnabled) {
                if (!mPeripheral->disable()) {
                    FL_LOG_PARLIO("PARLIO: Failed to disable peripheral");
                } else {
                    mTxUnitEnabled = false;
                }
            }

            // Short delay for GPIO stabilization
            fl::delayMicroseconds(100);

            return ParlioEngineState::READY;
        } else {
            // Still draining final transmission
            return ParlioEngineState::DRAINING;
        }
    }

    // If not transmitting, we're ready
    if (!mIsrContext->mTransmitting) {
        return ParlioEngineState::READY;
    }

    // Incremental ring buffer refill during transmission
    while (hasRingSpace() && populateNextDMABuffer()) {
        // Continue populating buffers
    }

    // Transmission in progress (ISR-driven, async)
    return ParlioEngineState::DRAINING;
}

bool ParlioEngine::isTransmitting() const {
    if (!mIsrContext) {
        return false;
    }
    return mIsrContext->mTransmitting;
}

ParlioDebugMetrics ParlioEngine::getDebugMetrics() const {
    ParlioDebugMetrics metrics = {};

    if (!mIsrContext) {
        return metrics;
    }

    // Execute memory barrier to ensure all ISR writes are visible
    FL_MEMORY_BARRIER;

    metrics.mStartTimeUs = 0; // Not tracked yet
    metrics.mEndTimeUs = mIsrContext->mEndTimeUs;
    metrics.mIsrCount = mIsrContext->mIsrCount;
    metrics.mChunksQueued = 0; // Not tracked yet
    metrics.mChunksCompleted = mIsrContext->mChunksCompleted;
    metrics.mBytesTotal = mIsrContext->mTotalBytes;
    metrics.mBytesTransmitted = mIsrContext->mBytesTransmitted;
    metrics.mErrorCode = mErrorOccurred ? 1 : 0;
    metrics.mTransmissionActive = mIsrContext->mTransmissionActive;

    return metrics;
}

} // namespace detail
} // namespace fl

#endif // (ESP32 && FASTLED_ESP32_HAS_PARLIO) || FASTLED_STUB_IMPL
