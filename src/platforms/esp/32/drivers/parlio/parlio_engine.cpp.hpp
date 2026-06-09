// IWYU pragma: private

/// @file parlio_engine.cpp
/// @brief PARLIO hardware abstraction layer (HAL) implementation for ESP32
///
/// This file contains the low-level PARLIO hardware management code extracted
/// from channel_driver_parlio.cpp. It handles all hardware-specific operations
/// including ISR callbacks, DMA buffer generation, and ring buffer streaming.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

// Compile on ESP32 with PARLIO support OR on stub platform for testing
#if (defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_PARLIO) || defined(FASTLED_STUB_IMPL)

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"  // For fl::u32
#include "platforms/memory_barrier.h"  // For FL_MEMORY_BARRIER

#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
#include "fl/channels/detail/wave3.hpp"
#include "fl/channels/detail/wave8.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_isr_context.h"
#include "platforms/esp/32/drivers/parlio/parlio_buffer_calc.h"
#include "platforms/esp/32/drivers/parlio/parlio_debug.h"
#include "platforms/esp/32/drivers/parlio/parlio_ring_buffer.h"
#include "fl/system/delay.h"

#include "fl/log/log.h"
#include "fl/log/log.h"
#include "fl/log/log.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/assert.h"
#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"

#ifdef FASTLED_STUB_IMPL
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h"
#else
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_esp.h"
#include "esp_heap_caps.h"
#endif

// All ESP-IDF dependencies have been abstracted through IParlioPeripheral interface
// - Timer operations: fl::isr::attachTimerHandler(), etc. (via fl/isr.h)
// - Timestamp operations: getMicroseconds()
// - Task management: TaskCoroutine (via fl/stl/task.h)
// - DMA memory: allocateDmaBuffer(), freeDmaBuffer()
// - PARLIO operations: initialize(), enable(), transmit(), waitAllDone(), etc.

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace {


fl::detail::IParlioPeripheral* getParlioPeripheral() FL_NOEXCEPT {
    // Get peripheral singleton instance
    #ifdef FASTLED_STUB_IMPL
        return &fl::detail::ParlioPeripheralMock::instance();
        FL_LOG_PARLIO("PARLIO_INIT: Using mock peripheral");
    #else
        return &fl::detail::ParlioPeripheralESP::instance();
        FL_LOG_PARLIO("PARLIO_INIT: Using ESP peripheral (real hardware)");
    #endif
}

bool parlioDmaPsramAvailable() FL_NOEXCEPT {
#ifdef FASTLED_STUB_IMPL
    return false;
#else
    constexpr fl::u32 caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    return heap_caps_get_largest_free_block(caps) > 0;
#endif
}

bool parlioCanAllocateInternalDmaRing(size_t bytes_per_buffer, size_t buffer_count) FL_NOEXCEPT {
#ifdef FASTLED_STUB_IMPL
    (void)bytes_per_buffer;
    (void)buffer_count;
    return true;
#else
    const size_t aligned_size = ((bytes_per_buffer + 63) / 64) * 64;
    const size_t required_total = aligned_size * buffer_count;
    constexpr fl::u32 caps = MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
    return heap_caps_get_largest_free_block(caps) >= aligned_size &&
           heap_caps_get_free_size(caps) >= required_total;
#endif
}

} // anonymous namespace

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

// PARLIO ISR priority level
// - Level 3 is the highest priority for C interrupt handlers on ESP32
// - Level 4+ requires assembly handlers and is not supported by ESP-IDF PARLIO driver
// - WiFi runs at level 4, so PARLIO ISRs may be preempted during WiFi activity
// - Configurable via build flag: -DFL_ESP_PARLIO_ISR_PRIORITY=<level>
#ifndef FL_ESP_PARLIO_ISR_PRIORITY
#define FL_ESP_PARLIO_ISR_PRIORITY 3  // Priority level 3 (recommended max for C handlers)
#endif

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

// #2548: when set, beginTransmission() submits all pre-encoded ring buffers
// to the IDF TX queue immediately FOR FRAMES THAT FIT IN THE RING. The IDF
// holds up to FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH pending transmits and chains
// them via hardware DMA descriptors — eliminating the ISR-latency gap between
// buffers that would otherwise serialize encode→TX within a frame.
//
// For frames LARGER than the ring (extra-long LED strips), the engine falls
// back to the chunked-streaming path: txDoneCallback + workerIsrCallback
// submit + populate buffers one at a time as TX drains, with new chunks
// encoded on the fly. Both paths coexist; the decision is per-frame.
//
// MEASURED IMPACT (16-lane × 256-LED WS2812B on P4 v1.3, fits-in-ring path):
//   off: total=16 995us  show=9 439us  wait=7 556us  (encode→TX serial)
//   on : total=12 220us  show=10 605us wait=1 615us  (encode/TX overlap, ~30% faster)
//
// Default ON. If a specific chipset/IDF combination shows WS2812 glitches
// from the inter-buffer chaining, set `-DFL_PARLIO_EAGER_QUEUE=0` to revert
// to the conservative one-submit-per-txDone pattern.
#ifndef FL_PARLIO_EAGER_QUEUE
#define FL_PARLIO_EAGER_QUEUE 1
#endif

// Worker function is now called directly from txDoneCallback (no timer needed)
// This eliminates the 50µs periodic timer overhead

//=============================================================================
// Buffer Population Helper - Shared Logic for DMA Buffer Byte Count Calculation
//=============================================================================

/// @brief Parameters for DMA buffer population byte count calculation
///
/// This struct encapsulates the inputs needed to calculate how many bytes
/// to process in a single DMA buffer population. Used by both workerIsrCallback()
/// and populateNextDMABuffer() to avoid code duplication.
struct BufferPopulationParams {
    size_t totalBytes;              ///< Total bytes to transmit (bytes per lane)
    size_t nextByteOffset;          ///< Current byte offset in transmission
    size_t ringCount;               ///< Number of buffers currently in ring
    size_t ringBufferCapacity;      ///< Capacity of each ring buffer
    size_t dataWidth;               ///< Data width (number of lanes: 1, 2, 4, 8, 16)
    size_t laneStride;              ///< Bytes per lane in scratch buffer
    u32 resetUs;               ///< Reset time in microseconds
    bool useWave3;                  ///< True if using wave3 encoding
    u32 clockFreqHz;               ///< Clock frequency in Hz
};

/// @brief Calculate the byte count for a DMA buffer population
///
/// This is a pure calculation function that determines how many input bytes
/// to process for the next DMA buffer. It handles:
/// - LED boundary alignment (3 bytes/LED × number of lanes)
/// - Ring buffer capacity limits
/// - Last buffer special case (takes all remaining bytes)
/// - Lane stride capping (prevents buffer overflow)
/// - Minimum buffer size enforcement (prevents < 15 byte buffers)
///
/// ISR-SAFE: No allocations, no logging, pure computation.
///
/// @param params Input parameters for calculation
/// @return Number of input bytes to process for this buffer
FL_OPTIMIZE_FUNCTION static inline size_t FL_IRAM
calculateBufferByteCount(const BufferPopulationParams& params) FL_NOEXCEPT {
    // Calculate remaining bytes
    size_t bytes_remaining = params.totalBytes - params.nextByteOffset;
    if (bytes_remaining == 0) {
        return 0;
    }

    // CRITICAL FIX (Iteration 3): Enforce minimum buffer size to prevent DMA timing glitches
    // Empirical testing on ESP32-C6 shows that buffers < 15 INPUT bytes cause 100% corruption
    // for single-lane configurations (data_width=1).
    // Root cause: DMA controller needs sufficient time to transition between ring buffers.
    // Threshold: 5 LEDs × 3 bytes/LED = 15 INPUT bytes minimum per buffer
    //
    // CRITICAL FIX (Iteration 5): Apply threshold AFTER data width expansion
    // Multi-lane configurations expand data size by outputBytesPerInputByte() factor:
    // - 1 lane (data_width=1):  8× expansion  → 15 input bytes = 120 encoded bytes
    // - 2 lanes (data_width=2): 16× expansion → 15 input bytes = 240 encoded bytes
    // - 4 lanes (data_width=4): 32× expansion → 15 input bytes = 480 encoded bytes
    // - 8 lanes (data_width=8): 64× expansion → 15 input bytes = 960 encoded bytes
    // The minimum buffer size must be checked on ENCODED output bytes, not input bytes.
    //
    // ITERATION 11 EXPERIMENT: Increase threshold from 15→20 input bytes
    // Hypothesis: 240 encoded bytes (at minimum) still causes glitches
    // Testing with 20 input bytes = 320 encoded bytes (33% safety margin)
    constexpr size_t MIN_INPUT_BYTES_PER_BUFFER = 20;  // Increased from 15 to test glitch hypothesis

    // Calculate buffer sizing parameters (used throughout function)
    ParlioBufferCalculator calc(params.dataWidth, params.useWave3, params.clockFreqHz); // ok no noexcept
    size_t expansionFactor = calc.outputBytesPerInputByte();

    // Calculate effective ring count to ensure minimum buffer size
    // For small LED counts, reduce ring count to avoid creating tiny buffers
    size_t effective_ring_count = ParlioRingBuffer3::RING_BUFFER_COUNT;
    size_t min_encoded_bytes = MIN_INPUT_BYTES_PER_BUFFER * expansionFactor;

    // If total bytes would create buffers < min_encoded_bytes, reduce ring count
    // CRITICAL: Apply expansion factor to get ACTUAL encoded buffer size
    while (effective_ring_count > 1) {
        size_t test_input_bytes_per_buffer = (params.totalBytes + effective_ring_count - 1) / effective_ring_count;
        size_t test_encoded_bytes_per_buffer = test_input_bytes_per_buffer * expansionFactor;
        if (test_encoded_bytes_per_buffer >= min_encoded_bytes) {
            break;  // Found acceptable ring count
        }
        effective_ring_count--;  // Try with fewer buffers
    }

    // ITERATION 11: 2-buffer bypass hypothesis FAILED
    // Forcing 2→3 buffers creates UNDERSIZED buffers (10 input bytes = 160 encoded < 240 minimum)
    // This caused 36.7μs glitch instead of 52.2μs glitch - DIFFERENT but still corrupt
    // Root cause is NOT buffer count - reverting to investigate buffer size threshold
    // (Removed 2-buffer bypass code)

    // Calculate target bytes per buffer (divide total into chunks)
    size_t bytes_per_buffer = (params.totalBytes + effective_ring_count - 1)
                              / effective_ring_count;

    // LED boundary alignment constant. totalBytes is bytes per lane, so one
    // LED boundary is 3 RGB bytes; output expansion accounts for dataWidth.
    size_t bytes_per_led_all_lanes = 3;

    // Calculate maximum input bytes that fit in one buffer
    // (reuse calc object from above to avoid duplication)
    size_t reset_padding = calc.resetPaddingBytes(params.resetUs);
    size_t fixed_overhead = calc.boundaryPaddingBytes() + reset_padding;
    if (fixed_overhead >= params.ringBufferCapacity) {
        return 0;
    }
    size_t available_capacity = params.ringBufferCapacity - fixed_overhead;
    size_t max_input_bytes_per_buffer = available_capacity / expansionFactor;

    // CRITICAL: If ALL data fits in one buffer, use single buffer to avoid DMA gap.
    // The PARLIO peripheral has a ~20µs gap between DMA buffer transitions
    // (hardware limitation) that corrupts WS2812 signal timing.
    // calculateRingBufferCapacity() already sizes buffers to hold full frames
    // when possible, so honor that by using effective_ring_count=1 here.
    if (max_input_bytes_per_buffer >= params.totalBytes) {
        effective_ring_count = 1;
        bytes_per_buffer = params.totalBytes;
    }

    // Cap bytes_per_buffer at ring buffer capacity
    if (bytes_per_buffer > max_input_bytes_per_buffer) {
        bytes_per_buffer = max_input_bytes_per_buffer;
    }

    // LED boundary alignment: Round DOWN to nearest multiple of (3 bytes × lane count)
    bytes_per_buffer = (bytes_per_buffer / bytes_per_led_all_lanes) * bytes_per_led_all_lanes;

    // Edge case: Ensure at least one LED across all lanes per buffer if data exists
    if (bytes_per_buffer < bytes_per_led_all_lanes && params.totalBytes >= bytes_per_led_all_lanes) {
        bytes_per_buffer = bytes_per_led_all_lanes;
    }

    // Determine if this is the last buffer (use effective ring count, not hardcoded constant)
    bool is_last_buffer = (params.ringCount >= effective_ring_count - 1) ||
                          (bytes_remaining <= bytes_per_buffer);

    // Calculate byte count for this buffer
    size_t byte_count;
    if (is_last_buffer) {
        // Last buffer takes all remaining bytes, but cap at buffer capacity
        byte_count = bytes_remaining;
        if (byte_count > max_input_bytes_per_buffer) {
            byte_count = max_input_bytes_per_buffer;
        }
    } else {
        byte_count = bytes_per_buffer;
    }

    // Handle edge case: zero byte_count when data remains
    // (can happen when bytes_per_buffer rounds down to 0 for partial LEDs)
    if (byte_count == 0 && bytes_remaining > 0) {
        byte_count = bytes_remaining;
        if (byte_count > max_input_bytes_per_buffer) {
            byte_count = max_input_bytes_per_buffer;
        }
    }

    // CRITICAL: Cap byte_count at lane stride to prevent buffer overflow
    // The populateDmaBuffer() function accesses mScratchBuffer using:
    //   mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
    // where byteOffset ranges from 0 to byteCount-1.
    // If byteCount > mLaneStride, then for lane > 0, the access will exceed buffer bounds.
    if (byte_count > params.laneStride) {
        byte_count = params.laneStride;
    }

    return byte_count;
}

//=============================================================================
// ParlioEngine - Singleton Implementation
//=============================================================================

ParlioEngine::ParlioEngine() FL_NOEXCEPT
    : mInitialized(false),
      mDataWidth(0),
      mActualChannels(0),
      mDummyLanes(0),
      mPeripheral(nullptr),
      mTimingT1Ns(0),
      mTimingT2Ns(0),
      mTimingT3Ns(0),
      mResetUs(0),
      mUseWave3(false),
      mClockFreqHz(8000000),
      mEncodingMode(EncodingMode::CLOCKLESS),
      mSpiClockHz(0),
      mIsrContext(),
      mMainTaskHandle(nullptr),
      mPollNeededCallback(),
      mRingBuffer(),
      mRingBufferCapacity(0),
      mScratchBuffer(nullptr),
      mLaneStride(0),
      mErrorOccurred(false),
      mTxUnitEnabled(false),
      mMaxLedsPerChannel(0) {
    FL_LOG_PARLIO("PARLIO_INIT: Constructor entered");
}

ParlioEngine::~ParlioEngine() {
    // Wait for any active transmissions to complete
    u32 start = mPeripheral ? mPeripheral->millis() : 0;
    while (isTransmitting()) {
        if (mPeripheral && (mPeripheral->millis() - start >= 2000)) {
            FL_LOG_PARLIO("PARLIO: Engine destructor timeout waiting for transmission");
            break;
        }
        if (mPeripheral) mPeripheral->delayMicroseconds(100);
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

#ifdef FL_DEBUG
    // Clean up debug task
    if (mDebugTask.is_valid()) {
        // Signal task to exit by setting flags to false (task checks: while (mTransmitting || mStreamComplete))
        if (mIsrContext) {
            mIsrContext->mTransmitting.store(false, fl::memory_order_release);
            mIsrContext->mStreamComplete.store(false, fl::memory_order_release);
        }
        // Give task time to self-delete (task calls fl::task::exit_current() at end)
        // Task sleeps for 500ms, so we need to wait at least 500ms for it to wake up and check flags
        // Use 600ms to be safe
        if (mPeripheral) {
            mPeripheral->delay(600);
        }

        // Task should have self-deleted by now (via fl::task::exit_current())
        // Cancel the task to release resources (task object cleaned up automatically)
        mDebugTask.cancel();
    }
#endif

    // Clean up ring buffer (unique_ptr handles deletion automatically)
    mRingBuffer.reset();

    // Clean up IsrContext (unique_ptr handles deletion automatically)
    mIsrContext.reset();

    // Clear state
    mPins.clear();
}

ParlioEngine& ParlioEngine::getInstance() FL_NOEXCEPT {
    return fl::Singleton<ParlioEngine>::instance();
}

void ParlioEngine::setPollNeededCallback(IChannelDriver::PollNeededCallback callback) FL_NOEXCEPT {
    mPollNeededCallback.set(callback);
}

void ParlioEngine::cleanup() FL_NOEXCEPT {
#ifdef FL_DEBUG
    // Clean up debug task only - singleton remains alive
    // This is called before DLL unload to properly join threads
    if (mDebugTask.is_valid()) {
        // Signal task to exit by setting flags to false
        if (mIsrContext) {
            mIsrContext->mTransmitting.store(false, fl::memory_order_release);
            mIsrContext->mStreamComplete.store(false, fl::memory_order_release);
        }
        // Give task time to self-delete (task sleeps for 500ms)
        if (mPeripheral) {
            mPeripheral->delay(600);
        }
        // Cancel the task to release resources
        mDebugTask.cancel();
    }
#endif
}

#ifdef FL_DEBUG
//=============================================================================
// Debug Task - Periodic ISR State Logging
//=============================================================================

/// @brief Low-priority debug task that periodically logs ISR state
/// @param arg Pointer to ParlioEngine instance
///
/// This task runs every 500ms during transmission and prints ISR debug counters
/// to help diagnose crashes and timing issues. Per LOOP.md iteration 5 instructions.
void ParlioEngine::debugTaskFunction(void* arg) FL_NOEXCEPT {
    auto *self = static_cast<ParlioEngine *>(arg);
    if (!self || !self->mIsrContext || !self->mPeripheral) {
        // Cannot proceed without valid context - self-delete
        fl::task::exit_current();
        return;  // UNREACHABLE on ESP32
    }

    ParlioIsrContext *ctx = self->mIsrContext.get();

    // Loop while transmission is active
    while (ctx->mTransmitting.load(fl::memory_order_acquire) ||
           ctx->mStreamComplete.load(fl::memory_order_acquire)) {
        // Wait 500ms between prints
        if (self->mPeripheral) {
            self->mPeripheral->delay(500);
        }

        const bool transmitting =
            ctx->mTransmitting.load(fl::memory_order_acquire);
        const bool stream_complete =
            ctx->mStreamComplete.load(fl::memory_order_acquire);
        const bool hardware_idle =
            ctx->mHardwareIdle.load(fl::memory_order_acquire);

        // Print ISR debug state (using FL_LOG_PARLIO which is safe from task context)
        FL_LOG_PARLIO("ISR_STATE:"
               << " txDone=" << ctx->mDebugTxDoneCount
               << " worker=" << ctx->mDebugWorkerIsrCount
               << " ring_count=" << ctx->mRingCount
               << " bytes_tx=" << ctx->mBytesTransmitted << "/" << ctx->mTotalBytes
               << " transmitting=" << (transmitting ? "YES" : "NO")
               << " complete=" << (stream_complete ? "YES" : "NO")
               << " hw_idle=" << (hardware_idle ? "YES" : "NO"));

        // Check for ring buffer underrun (CRITICAL ERROR per LOOP.md)
        if (hardware_idle && ctx->mRingCount == 0 && transmitting) {
            FL_ERROR("PARLIO: BUFFER UNDERRUN DETECTED - Hardware idle with empty ring during transmission"
                    << " | bytes_tx=" << ctx->mBytesTransmitted << "/" << ctx->mTotalBytes);
            // DO NOT restart PARLIO - per LOOP.md this is a critical error
            // Exit task and let main thread detect the error
            break;
        }

        // Exit if transmission complete
        if (stream_complete && !transmitting) {
            break;
        }
    }

    // Task is shutting down - self-delete
    fl::task::exit_current();
    // UNREACHABLE CODE on ESP32
}
#endif  // FL_DEBUG

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
                              const void *edata, void *user_ctx) FL_NOEXCEPT {
    // ⚠️  ISR CONTEXT - NO LOGGING ALLOWED - SEE FUNCTION HEADER ⚠️
    (void)tx_unit;  // Opaque handle (not used in this callback)
    (void)edata;

    auto *self = static_cast<ParlioEngine *>(user_ctx);
    if (!self || !self->mIsrContext) {
        return false;
    }

    // ISR Context - no logging allowed per linter rules
    // Even in stub mode, ISR functions cannot use FL_WARN/FL_DBG/FL_LOG_PARLIO

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
    if (ctx->mBytesTransmitted + input_bytes > ctx->mTotalBytes) {
        input_bytes = (ctx->mBytesTransmitted < ctx->mTotalBytes)
                          ? (ctx->mTotalBytes - ctx->mBytesTransmitted)
                          : 0;
    }
    ctx->mBytesTransmitted += input_bytes;
    ctx->mCurrentByte += input_bytes;
    ctx->mChunksCompleted++;

    // ⚠️  NO LOGGING IN ISR - Logging causes watchdog timeouts and crashes
    // Use GPIO toggling or counters for debug instead

    // ISR-based streaming: Check if next buffer is ready in the ring (use count to detect empty ring)
    volatile size_t count = ctx->mRingCount;

    // Ring empty - check if all data transmitted
    if (count == 0) {
        // Ring is empty - check if we've transmitted ALL the data (use >= for tolerance)
        // CRITICAL (Iteration 3): Check completion BEFORE overflow check
        // Overflow can happen due to LED boundary rounding, but we still need to complete
        if (ctx->mBytesTransmitted >= ctx->mTotalBytes) {
            // All data transmitted - mark transmission complete
            ctx->mTransmissionActive.store(false, fl::memory_order_release);
            ctx->mHardwareIdle.store(false, fl::memory_order_release);
            ctx->mEndTimeUs = ctx->mDebugLastTxDoneTime;

            // SAFETY CHECK: Detect byte overflow (tolerate small overflows from LED rounding)
            // Flag overflow for debugging, but don't prevent completion
            if (ctx->mBytesTransmitted > ctx->mTotalBytes) {
                ctx->mRingError.store(true, fl::memory_order_release);
            }
            ctx->mStreamComplete.store(true, fl::memory_order_release);
            self->mPollNeededCallback.invoke();

            // ASYNC MODE: No task notifications
            // - Worker task was removed (uses timer ISR instead)
            // - Main task polls mStreamComplete flag via poll() method
            // All task notifications removed to support fully async operation

            return false; // No tasks woken
        }

        // Ring empty but more data pending - call worker function directly
        ctx->mHardwareIdle.store(true, fl::memory_order_release);
        ctx->mUnderrunCount = ctx->mUnderrunCount + 1;

        // ONE-SHOT PATTERN: Call worker function directly to populate next buffer
        // This eliminates the 50µs periodic timer overhead
        workerIsrCallback(user_ctx);

        return false;
    }

    // Next buffer is ready - submit it to hardware
    size_t buffer_idx = read_idx;
    u8 *buffer_ptr = self->mRingBuffer->ptrs[buffer_idx];  // Use cached pointer (ISR optimization)
    size_t buffer_size = self->mRingBuffer->sizes[buffer_idx];

    // Invalid buffer - set error flag
    if (!buffer_ptr || buffer_size == 0) {
        ctx->mRingError.store(true, fl::memory_order_release);
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
        ctx->mHardwareIdle.store(false, fl::memory_order_release);

        // ONE-SHOT PATTERN: Call worker function directly if more buffers needed
        // This eliminates the 50µs periodic timer overhead
        if (ctx->mRingCount < ParlioRingBuffer3::RING_BUFFER_COUNT &&
            ctx->mNextByteOffset < ctx->mTotalBytes) {
            // Populate next buffer immediately
            workerIsrCallback(user_ctx);
        }

        // ASYNC MODE: Worker task notification removed
        // Worker populates buffers via continuous timer ISR (workerIsrCallback)
        // Doorbell flag signals when work is available

        return false; // No tasks woken
    } else {
        // Submission failed - set error flag for CPU to detect
        ctx->mRingError.store(true, fl::memory_order_release);
    }

    return false; // No high-priority task woken
}
// ============================================================================
// END OF ISR - Remember: NO LOGGING, NO BLOCKING, MINIMIZE EXECUTION TIME
// ============================================================================

//=============================================================================
// Worker Function - DMA Buffer Population (called from txDoneCallback)
//=============================================================================

// ============================================================================
// ⚠️ ⚠️ ⚠️  CRITICAL ISR SAFETY RULES - READ BEFORE MODIFYING ⚠️ ⚠️ ⚠️
// ============================================================================
//
// This function runs in INTERRUPT CONTEXT (called from txDoneCallback ISR)
// with EXTREMELY strict constraints:
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
//    - Keep execution as short as possible (ideally <10µs)
//    - Early exit if no work available
//    - Populate only ONE buffer per invocation
//
// 4. ✅ MEMORY BARRIERS
//    - Use FL_MEMORY_BARRIER after state updates
//    - Ensures visibility across ISR invocations
//
// If the system crashes after you modify this function:
// - First suspect: Did you add logging?
// - Second suspect: Did you add blocking operations?
// - Third suspect: Did you increase execution time?
//
// ============================================================================
FL_OPTIMIZE_FUNCTION void FL_IRAM
ParlioEngine::workerIsrCallback(void *user_data) FL_NOEXCEPT {
    // ⚠️  ISR CONTEXT - NO LOGGING ALLOWED - SEE FUNCTION HEADER ⚠️

    // Null checks first
    auto *self = static_cast<ParlioEngine *>(user_data);
    if (!self || !self->mIsrContext) {
        return;
    }

    ParlioIsrContext *ctx = self->mIsrContext.get();

    // Debug: Increment workerIsrCallback counter and timestamp
    ctx->mDebugWorkerIsrCount = ctx->mDebugWorkerIsrCount + 1;
    ctx->mDebugLastWorkerIsrTime = self->mPeripheral->getMicroseconds();

    // ISR Context - no logging allowed per linter rules
    // Even in stub mode, ISR functions cannot use FL_WARN/FL_DBG/FL_LOG_PARLIO

    // CRITICAL: Early exit checks (in order of likelihood)

    // Check 1: Ring buffer full (no space to populate)
    if (ctx->mRingCount >= ParlioRingBuffer3::RING_BUFFER_COUNT) {
        return;
    }

    // Check 2: All data already processed
    if (ctx->mNextByteOffset >= ctx->mTotalBytes) {
        return;
    }

    // Work available - populate ONE buffer
    // Get next ring buffer index (0-2)
    size_t ring_index = ctx->mRingWriteIdx;

    // Get ring buffer pointer (use cached pointer for optimization)
    u8 *outputBuffer = self->mRingBuffer->ptrs[ring_index];
    if (!outputBuffer) {
        return;  // Invalid buffer - should never happen
    }

    // Calculate byte count using shared helper function
    // This centralizes the LED boundary alignment, capacity limits, and lane stride capping logic
    BufferPopulationParams params;
    params.totalBytes = ctx->mTotalBytes;
    params.nextByteOffset = ctx->mNextByteOffset;
    params.ringCount = ctx->mRingCount;
    params.ringBufferCapacity = self->mRingBufferCapacity;
    params.dataWidth = self->mDataWidth;
    params.laneStride = self->mLaneStride;
    params.resetUs = self->mResetUs;
    params.useWave3 = self->mUseWave3;
    params.clockFreqHz = self->mClockFreqHz;

    size_t byte_count = calculateBufferByteCount(params);
    if (byte_count == 0) {
        return;  // No data to process
    }

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
ParlioEngine::populateDmaBuffer(u8* outputBuffer,
                                 size_t outputBufferCapacity,
                                 size_t startByte,
                                 size_t byteCount,
                                 size_t& outputBytesWritten) FL_NOEXCEPT {
    // SPI mode: delegate to SPI-specific encoding
    if (mEncodingMode == EncodingMode::SPI) {
        return populateDmaBufferSpi(outputBuffer, outputBufferCapacity,
                                     startByte, byteCount, outputBytesWritten);
    }

    // === Clockless (Wave8 or Wave3) encoding follows ===
    size_t outputIdx = 0;
    size_t byteOffset = 0;

    // Use calculator for padding and transpose block size
    ParlioBufferCalculator calc(mDataWidth, mUseWave3, mClockFreqHz); // ok no noexcept
    size_t blockSize = calc.transposeBlockSize();

    // ═══════════════════════════════════════════════════════════════
    // PHASE 1: Insert front padding
    // ═══════════════════════════════════════════════════════════════
    // Only add front padding on the FIRST byte of transmission
    bool is_first_byte = (startByte == 0);

    if (is_first_byte) {
        // ITERATION 18: Testing zero padding to check for phase shift issues
        constexpr size_t FRONT_PAD_BYTES = 0;  // NO front padding
        size_t front_padding_total = FRONT_PAD_BYTES * mDataWidth;

        // Boundary check
        if (outputIdx + front_padding_total > outputBufferCapacity) {
            // Buffer overflow - cannot log in hot path (causes 98× slowdown)
            // Error will be detected by caller via return false
            outputBytesWritten = outputIdx;
            return false;
        }

        if (front_padding_total > 0) {
            fl::isr::memset_zero(outputBuffer + outputIdx, front_padding_total);
        }
        outputIdx += front_padding_total;
    }

    // Process one byte position at a time using wave transpose functions
    // These functions combine expansion + transposition in one step with minimal memory overhead

    while (byteOffset < byteCount) {
        // Check if enough space for this block
        if (outputIdx + blockSize > outputBufferCapacity) {
            // Buffer overflow - return error immediately
            outputBytesWritten = outputIdx;
            return false;
        }

        // Prepare lane bytes for transposition
        // Use stack-allocated temporary arrays sized for the actual data width
        // Dispatch to appropriate transpose function based on data width and wave mode

        if (mUseWave3) {
            // === Wave3 encoding path ===
            if (mDataWidth == 1) {
                u8 byte_value = (mActualChannels > 0)
                    ? mScratchBuffer[0 * mLaneStride + startByte + byteOffset]
                    : 0;

                Wave3Byte wave3_output;
                fl::detail::wave3_convert_byte_to_wave3byte(byte_value, mWave3Lut, &wave3_output);

                fl::memcpy(outputBuffer + outputIdx, &wave3_output, sizeof(Wave3Byte));
                outputIdx += sizeof(Wave3Byte);
            } else if (mDataWidth == 2) {
                u8 lanes[2];
                lanes[0] = (mActualChannels > 1)
                    ? mScratchBuffer[1 * mLaneStride + startByte + byteOffset]
                    : 0;
                lanes[1] = (mActualChannels > 0)
                    ? mScratchBuffer[0 * mLaneStride + startByte + byteOffset]
                    : 0;

                // #2548 L2: write transpose result directly into DMA buffer
                // (capacity pre-validated by the `outputIdx + blockSize > outputBufferCapacity`
                //  check at the top of this loop). Skips the 128-B stack round-trip.
                fl::wave3Transpose_2(reinterpret_cast<const u8(&)[2]>(lanes), mWave3Lut, // ok reinterpret cast - array reference type conversion
                                    *reinterpret_cast<u8(*)[2 * sizeof(Wave3Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 L2)
                outputIdx += blockSize;
            } else if (mDataWidth == 4) {
                u8 lanes[4];
                for (size_t lane = 0; lane < 4; lane++) {
                    lanes[lane] = (lane < mActualChannels)
                        ? mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
                        : 0;
                }

                fl::wave3Transpose_4(reinterpret_cast<const u8(&)[4]>(lanes), mWave3Lut, // ok reinterpret cast - array reference type conversion
                                    *reinterpret_cast<u8(*)[4 * sizeof(Wave3Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 L2)
                outputIdx += blockSize;
            } else if (mDataWidth == 8) {
                u8 lanes[8];
                for (size_t lane = 0; lane < 8; lane++) {
                    lanes[lane] = (lane < mActualChannels)
                        ? mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
                        : 0;
                }

                fl::wave3Transpose_8(reinterpret_cast<const u8(&)[8]>(lanes), mWave3Lut, // ok reinterpret cast - array reference type conversion
                                    *reinterpret_cast<u8(*)[8 * sizeof(Wave3Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 L2)
                outputIdx += blockSize;
            } else if (mDataWidth == 16) {
                u8 lanes[16];
                for (size_t lane = 0; lane < 16; lane++) {
                    lanes[lane] = (lane < mActualChannels)
                        ? mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
                        : 0;
                }

                fl::wave3Transpose_16(reinterpret_cast<const u8(&)[16]>(lanes), mWave3Lut, // ok reinterpret cast - array reference type conversion
                                     *reinterpret_cast<u8(*)[16 * sizeof(Wave3Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 L2)
                outputIdx += blockSize;
            } else {
                outputBytesWritten = outputIdx;
                return false;
            }
        } else {
            // === Wave8 encoding path ===
            if (mDataWidth == 1) {
                // Single-lane mode: Convert byte directly to Wave8Byte WITHOUT interleaving
                // For single-lane, we don't want 2-lane transpose which would interleave
                // lane[0] with dummy lane[1] padding
                u8 byte_value = (mActualChannels > 0)
                    ? mScratchBuffer[0 * mLaneStride + startByte + byteOffset]
                    : 0;

                Wave8Byte wave8_output;
                fl::detail::wave8_expand_byte(byte_value, mWave8ByteLut, &wave8_output);

                // MSB bit packing: Wave8Bit stores pulses MSB-first, PARLIO MSB packing required
                // (Hardware validation confirms MSB packing is correct for Wave8 format)

                fl::memcpy(outputBuffer + outputIdx, &wave8_output, sizeof(Wave8Byte));
                outputIdx += sizeof(Wave8Byte);
            } else if (mDataWidth == 2) {
                // Two-lane mode: Use 2-lane transpose to interleave lanes
                // CRITICAL: FL_WAVE8_SPREAD_TO_16 puts lanes[0] in ODD bits → data_line[1],
                // and lanes[1] in EVEN bits → data_line[0].
                // PARLIO maps data_line[0]=pins[0]=channel0_pin, data_line[1]=pins[1]=channel1_pin.
                // To send channel 0's data on channel 0's pin (data_line[0] = EVEN bits),
                // channel 0 data must go in lanes[1] and channel 1 data in lanes[0].
                u8 lanes[2];
                lanes[0] = (mActualChannels > 1)
                    ? mScratchBuffer[1 * mLaneStride + startByte + byteOffset]
                    : 0;
                lanes[1] = (mActualChannels > 0)
                    ? mScratchBuffer[0 * mLaneStride + startByte + byteOffset]
                    : 0;

                // #2548 L2: write transpose result directly into DMA buffer
                // (capacity pre-validated by the `outputIdx + blockSize > outputBufferCapacity`
                //  check at the top of this loop). Skips the 128-B stack round-trip.
                // #2548 BF1: chipset-aware direct encode (skips byte_lut).
                fl::wave8Transpose_2_bf1(reinterpret_cast<const u8(&)[2]>(lanes), mWave8ByteLut, // ok reinterpret cast - array reference type conversion
                                    *reinterpret_cast<u8(*)[2 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 BF1)
                outputIdx += blockSize;
            } else if (mDataWidth == 4) {
                u8 lanes[4];
                for (size_t lane = 0; lane < 4; lane++) {
                    lanes[lane] = (lane < mActualChannels)
                        ? mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
                        : 0;
                }

                // #2548 BF1: chipset-aware direct encode (skips byte_lut).
                fl::wave8Transpose_4_bf1(reinterpret_cast<const u8(&)[4]>(lanes), mWave8ByteLut, // ok reinterpret cast - array reference type conversion
                                    *reinterpret_cast<u8(*)[4 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 BF1)
                outputIdx += blockSize;
            } else if (mDataWidth == 8) {
                u8 lanes[8];
                for (size_t lane = 0; lane < 8; lane++) {
                    lanes[lane] = (lane < mActualChannels)
                        ? mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
                        : 0;
                }

                // #2548 BF1: chipset-aware direct encode (skips byte_lut).
                fl::wave8Transpose_8_bf1(reinterpret_cast<const u8(&)[8]>(lanes), mWave8ByteLut, // ok reinterpret cast - array reference type conversion
                                    *reinterpret_cast<u8(*)[8 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 BF1)
                outputIdx += blockSize;
            } else if (mDataWidth == 16) {
                u8 lanes_a[16];
                for (size_t lane = 0; lane < 16; lane++) {
                    lanes_a[lane] = (lane < mActualChannels)
                        ? mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
                        : 0;
                }

                // #2548 BF1 dispatch (chipset-aware direct encode, deep-dive):
                //   ≥4 remaining → wave8Transpose_16x4_bf1_pipe4 (5.49× vs baseline)
                //   ≥1 remaining → wave8Transpose_16_bf1 (2.16× vs baseline)
                // Both bypass the byte_lut expand stage entirely — they call
                // spread_transpose16_symbol ONCE per byte-position on the input
                // bytes directly (vs current 8 calls + 16 byte-LUT loads). The
                // bf1_pipe4 fusion also gives cross-position ILP for free.
                // Bit-identical to wave8Transpose_16 for any Wave8 chipset.
                if (byteOffset + 3 < byteCount
                    && outputIdx + 4 * blockSize <= outputBufferCapacity) {
                    u8 lanes_b[16];
                    u8 lanes_c[16];
                    u8 lanes_d[16];
                    for (size_t lane = 0; lane < 16; lane++) {
                        const u8 *base = mScratchBuffer + lane * mLaneStride + startByte;
                        const bool active = (lane < mActualChannels);
                        lanes_b[lane] = active ? base[byteOffset + 1] : 0;
                        lanes_c[lane] = active ? base[byteOffset + 2] : 0;
                        lanes_d[lane] = active ? base[byteOffset + 3] : 0;
                    }
                    fl::wave8Transpose_16x4_bf1_pipe4(
                        reinterpret_cast<const u8(&)[16]>(lanes_a), // ok reinterpret cast - array reference type conversion
                        reinterpret_cast<const u8(&)[16]>(lanes_b), // ok reinterpret cast - array reference type conversion
                        reinterpret_cast<const u8(&)[16]>(lanes_c), // ok reinterpret cast - array reference type conversion
                        reinterpret_cast<const u8(&)[16]>(lanes_d), // ok reinterpret cast - array reference type conversion
                        mWave8ByteLut,
                        *reinterpret_cast<u8(*)[16 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx),                      // ok reinterpret cast - direct write to DMA buffer (#2548)
                        *reinterpret_cast<u8(*)[16 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx + blockSize),          // ok reinterpret cast - direct write to DMA buffer (#2548)
                        *reinterpret_cast<u8(*)[16 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx + 2 * blockSize),      // ok reinterpret cast - direct write to DMA buffer (#2548)
                        *reinterpret_cast<u8(*)[16 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx + 3 * blockSize));     // ok reinterpret cast - direct write to DMA buffer (#2548)
                    outputIdx += 4 * blockSize;
                    byteOffset += 3;  // outer loop also does ++byteOffset → net +4
                } else {
                    fl::wave8Transpose_16_bf1(
                        reinterpret_cast<const u8(&)[16]>(lanes_a), mWave8ByteLut, // ok reinterpret cast - array reference type conversion
                        *reinterpret_cast<u8(*)[16 * sizeof(Wave8Byte)]>(outputBuffer + outputIdx)); // ok reinterpret cast - direct write to DMA buffer (#2548 BF1)
                    outputIdx += blockSize;
                }
            } else {
                // Invalid data width - should never happen (validated in initialize())
                outputBytesWritten = outputIdx;
                return false;
            }
        }

        byteOffset++;
    }

    // ═══════════════════════════════════════════════════════════════
    // STAGE 3: Append back padding
    // ═══════════════════════════════════════════════════════════════
    // Only append back padding on the LAST byte of transmission
    // (when processing the final byte in the total byte range)
    bool is_last_byte = (startByte + byteCount >= mIsrContext->mTotalBytes);

    if (is_last_byte) {
        size_t back_pad_bytes = mUseWave3 ? 3 : 8;  // 1 Wave3Byte or 1 Wave8Byte per lane
        size_t back_padding_total = back_pad_bytes * mDataWidth;

        // Boundary check
        if (outputIdx + back_padding_total > outputBufferCapacity) {
            // Buffer overflow - cannot log in hot path (causes 98× slowdown)
            // Error will be detected by caller via return false
            outputBytesWritten = outputIdx;
            return false;
        }

        fl::isr::memset_zero(outputBuffer + outputIdx, back_padding_total);
        outputIdx += back_padding_total;
    }

    // ═══════════════════════════════════════════════════════════════
    // STAGE 4: Append reset time padding (all-zero bytes)
    // ═══════════════════════════════════════════════════════════════
    // Only append reset padding on the LAST byte of transmission
    // (when processing the final byte in the total byte range)

    if (is_last_byte && mResetUs > 0) {
        // Calculate reset padding bytes needed (uses wave mode and clock freq)
        size_t reset_padding_bytes = calc.resetPaddingBytes(mResetUs);

        // Boundary check: Ensure padding fits in output buffer
        if (outputIdx + reset_padding_bytes > outputBufferCapacity) {
            // Buffer overflow - cannot log in hot path (causes 98× slowdown)
            // Error will be detected by caller via return false
            outputBytesWritten = outputIdx;
            return false;
        }

        // Append all-zero bytes (LOW signal for reset duration)
        fl::isr::memset_zero(outputBuffer + outputIdx, reset_padding_bytes);
        outputIdx += reset_padding_bytes;
    }

    outputBytesWritten = outputIdx;
    return true;
}

//=============================================================================
// Ring Buffer Management - Incremental Population Architecture
//=============================================================================

bool ParlioEngine::hasRingSpace() const FL_NOEXCEPT {
    if (!mIsrContext) {
        return false;
    }

    // Use count to determine if ring has space (distinguishes full vs empty)
    volatile size_t count = mIsrContext->mRingCount;

    // Ring has space if count is less than ParlioRingBuffer3::RING_BUFFER_COUNT
    return count < ParlioRingBuffer3::RING_BUFFER_COUNT;
}


bool ParlioEngine::allocateRingBuffers() FL_NOEXCEPT {
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

    u8* buffers[3] = {nullptr, nullptr, nullptr};

    if (!parlioDmaPsramAvailable() &&
        !parlioCanAllocateInternalDmaRing(mRingBufferCapacity,
                                          ParlioRingBuffer3::RING_BUFFER_COUNT)) {
        FL_LOG_PARLIO("PARLIO: Insufficient DMA heap for "
                      << ParlioRingBuffer3::RING_BUFFER_COUNT
                      << " ring buffers of " << mRingBufferCapacity << " bytes");
        return false;
    }

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

        // Note: No memset needed - heap_caps_aligned_calloc already zeros memory
    }

    // Create ring buffer with unique_ptr and cleanup callback
    // Capture mPeripheral pointer for cleanup lambda
    auto* peripheral = mPeripheral;
    mRingBuffer = fl::make_unique<ParlioRingBuffer3>(
        buffers[0], buffers[1], buffers[2],
        mRingBufferCapacity,
        [peripheral](u8* ptr) FL_NOEXCEPT {
            peripheral->freeDmaBuffer(ptr);
        }
    );

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
ParlioEngine::populateNextDMABuffer() FL_NOEXCEPT {
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
    u8 *outputBuffer = mRingBuffer->ptrs[ring_index];
    if (!outputBuffer) {
        //FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        mErrorOccurred = true;
        return false;
    }

    // Calculate byte count using shared helper function
    // This centralizes the LED boundary alignment, capacity limits, and lane stride capping logic
    // See calculateBufferByteCount() for detailed documentation of the algorithm.
    BufferPopulationParams params;
    params.totalBytes = mIsrContext->mTotalBytes;
    params.nextByteOffset = mIsrContext->mNextByteOffset;
    params.ringCount = mIsrContext->mRingCount;
    params.ringBufferCapacity = mRingBufferCapacity;
    params.dataWidth = mDataWidth;
    params.laneStride = mLaneStride;
    params.resetUs = mResetUs;
    params.useWave3 = mUseWave3;
    params.clockFreqHz = mClockFreqHz;

    size_t byte_count = calculateBufferByteCount(params);

    // SAFETY CHECK: Never exceed total bytes (prevents overflow from rounding errors)
    // This additional check is specific to populateNextDMABuffer (not in workerIsr)
    if (mIsrContext->mNextByteOffset + byte_count > mIsrContext->mTotalBytes) {
        byte_count = mIsrContext->mTotalBytes - mIsrContext->mNextByteOffset;
    }

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
    if (mIsrContext->mHardwareIdle.load(fl::memory_order_acquire)) {
        // Get the buffer that was just populated (read_idx points to next buffer to transmit)
        size_t buffer_idx = mIsrContext->mRingReadIdx;
        u8 *buffer_ptr = mRingBuffer->ptrs[buffer_idx];  // Use cached pointer for optimization
        size_t buffer_size = mRingBuffer->sizes[buffer_idx];

        if (buffer_ptr && buffer_size > 0) {
            // Submit buffer to hardware to restart transmission via peripheral interface
            bool success = mPeripheral->transmit(buffer_ptr, buffer_size * 8, 0x0000);

            if (success) {
                // Successfully restarted - advance read index and decrement count
                mIsrContext->mRingReadIdx = (mIsrContext->mRingReadIdx + 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;
                mIsrContext->mRingCount = mIsrContext->mRingCount - 1;
                mIsrContext->mHardwareIdle.store(false, fl::memory_order_release);
                mIsrContext->mTransmitting.store(true, fl::memory_order_release);
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
                              size_t maxLedsPerChannel) FL_NOEXCEPT {
    FL_LOG_PARLIO("PARLIO_INIT: initialize() called - dataWidth=" << dataWidth << " pins=" << pins.size());

    // Get peripheral first (needed for both initial and re-initialization)
    if (mPeripheral == nullptr) {
        mPeripheral = getParlioPeripheral();
    }

    // Check if already initialized AND peripheral is still initialized AND configuration matches
    // This handles the case where:
    // 1. mock.reset() was called but engine still thinks it's initialized
    // 2. Parameters changed (different dataWidth, pins, etc.) requiring reconfiguration
    // CRITICAL: Must compare configuration to avoid buffer overflows when tests
    // change lane count without calling mock.reset()
    bool config_matches = (mDataWidth == dataWidth &&
                          mPins.size() == pins.size() &&
                          mTimingT1Ns == timing.t1_ns &&
                          mTimingT2Ns == timing.t2_ns &&
                          mTimingT3Ns == timing.t3_ns);
    if (mInitialized && mPeripheral && mPeripheral->isInitialized() && config_matches) {
        // Config matches but check if ring buffers need reallocation for larger LED count
        if (maxLedsPerChannel > mMaxLedsPerChannel) {
            FL_LOG_PARLIO("PARLIO_INIT: Config matches but maxLeds increased from "
                         << mMaxLedsPerChannel << " to " << maxLedsPerChannel
                         << " - reallocating ring buffers");

            // Recalculate ring buffer capacity for larger LED count
            ParlioBufferCalculator calc(mDataWidth, mUseWave3, mClockFreqHz); // ok no noexcept
            const bool psram_cap_available = parlioDmaPsramAvailable();
            size_t raw_capacity = calc.calculateRingBufferCapacity(
                maxLedsPerChannel, mResetUs, ParlioRingBuffer3::RING_BUFFER_COUNT,
                psram_cap_available ? FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM
                                     : FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES);
            size_t new_capacity = ((raw_capacity + 63) / 64) * 64;

            if (new_capacity > mRingBufferCapacity) {
                mRingBufferCapacity = new_capacity;
                // Release old ring buffers before allocating new ones
                mRingBuffer.reset();
                if (!allocateRingBuffers()) {
                    if (psram_cap_available) {
                        // PSRAM-sized allocation failed, retry with SRAM cap
                        raw_capacity = calc.calculateRingBufferCapacity(
                            maxLedsPerChannel, mResetUs, ParlioRingBuffer3::RING_BUFFER_COUNT,
                            FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES);
                        mRingBufferCapacity = ((raw_capacity + 63) / 64) * 64;
                    }
                    if (!psram_cap_available || !allocateRingBuffers()) {
                        FL_LOG_PARLIO("PARLIO_INIT: FAILED to reallocate ring buffers");
                        mInitialized = false;
                        return false;
                    }
                }
                FL_LOG_PARLIO("PARLIO_INIT: Ring buffers reallocated (capacity=" << mRingBufferCapacity << ")");
            }
            mMaxLedsPerChannel = maxLedsPerChannel;
        }

        // CRITICAL: Reinitialize the PARLIO TX unit when reusing the same config.
        // The ESP32 PARLIO TX unit's internal DMA state machine can get stuck after
        // channels are destroyed and recreated. A full deinit/reinit of the TX unit
        // resets this state while keeping ring buffers allocated (avoiding fragmentation).
        FL_LOG_PARLIO("PARLIO_INIT: Reinitializing TX unit for clean DMA state");
        mPeripheral->deinitialize();
        mTxUnitEnabled = false;

        ParlioPeripheralConfig config(
            pins,
            mClockFreqHz,
            FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH,
            65534,
            #if PARLIO_FORCE_LSB_MODE
            ParlioBitPackOrder::FL_PARLIO_LSB,
            #else
            ParlioBitPackOrder::FL_PARLIO_MSB,
            #endif
            parlioDmaPsramAvailable()
        );
        if (!mPeripheral->initialize(config)) {
            FL_LOG_PARLIO("PARLIO_INIT: FAILED to reinitialize TX unit");
            mInitialized = false;
            return false;
        }
        if (!mPeripheral->registerTxDoneCallback(
                reinterpret_cast<void*>(txDoneCallback), this)) { // ok reinterpret cast - callback function pointer to void*
            FL_LOG_PARLIO("PARLIO_INIT: FAILED to re-register ISR callback");
            mPeripheral->deinitialize();
            mInitialized = false;
            return false;
        }

        FL_LOG_PARLIO("PARLIO_INIT: TX unit reinitialized, config still matching");
        return true;
    }

    // If configuration changed, we need to re-initialize
    if (mInitialized && !config_matches) {
        FL_LOG_PARLIO("PARLIO_INIT: Configuration changed, re-initializing");
        mInitialized = false;
        mTxUnitEnabled = false;
        // Free old ring buffers BEFORE allocating new ones to avoid peak
        // memory of 6×capacity (old 3 + new 3). On ESP32-C6 without PSRAM,
        // the doubled allocation at 4-lane×100 LEDs (~57KB) exceeds free heap.
        mRingBuffer.reset();
    }

    // If we reach here, either first initialization or peripheral was reset
    // Reset engine state to allow re-initialization
    if (mInitialized && mPeripheral && !mPeripheral->isInitialized()) {
        FL_LOG_PARLIO("PARLIO_INIT: Peripheral was reset, re-initializing");
        mInitialized = false;
        mTxUnitEnabled = false;  // Reset enable state to match peripheral
    }
    // Store data width and pins
    mDataWidth = dataWidth;
    mPins = pins;
    // Count only real (non-negative) pins - dummy lanes use pin=-1
    mActualChannels = 0;
    for (size_t i = 0; i < pins.size(); i++) {
        if (pins[i] >= 0) {
            mActualChannels++;
        }
    }
    mDummyLanes = mDataWidth - mActualChannels;

    FL_LOG_PARLIO("PARLIO_INIT: mDataWidth=" << mDataWidth << " mActualChannels=" << mActualChannels << " mDummyLanes=" << mDummyLanes);

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

    // Validate pin count matches data width
    if (pins.size() != dataWidth) {
        FL_LOG_PARLIO("PARLIO: Pin configuration error - expected "
                << dataWidth << " pins, got " << pins.size());
        return false;
    }

    // Build expansion LUT from timing configuration
    ChipsetTiming chipsetTiming;
    chipsetTiming.T1 = mTimingT1Ns;
    chipsetTiming.T2 = mTimingT2Ns;
    chipsetTiming.T3 = mTimingT3Ns;
    chipsetTiming.RESET = mResetUs;  // Stored for documentation (padding handled in DMA buffer population)
    chipsetTiming.name = "PARLIO";

    // Check if wave3 encoding is eligible for this chipset timing
    mUseWave3 = canUseWave3(chipsetTiming);
    if (mUseWave3) {
        mClockFreqHz = wave3ClockFrequencyHz(chipsetTiming);
        mWave3Lut = buildWave3ExpansionLUT(chipsetTiming);
        FL_LOG_PARLIO("PARLIO_INIT: Wave3 mode selected (clock=" << mClockFreqHz << " Hz)");
    } else {
        mClockFreqHz = FL_ESP_PARLIO_CLOCK_FREQ_HZ;
        FL_LOG_PARLIO("PARLIO_INIT: Wave8 mode selected (clock=" << mClockFreqHz << " Hz)");
    }
    // Always build wave8 LUT (needed as fallback and for SPI mode)
    mWave8Lut = buildWave8ExpansionLUT(chipsetTiming);
    // #2526: also build the byte-indexed LUT used by the hot encode path.
    mWave8ByteLut = buildWave8ByteExpansionLUT(mWave8Lut);
    mEncodingMode = EncodingMode::CLOCKLESS;

    // Configure peripheral (constructor handles -1 filling for unused pins)
    const bool psram_cap_available = parlioDmaPsramAvailable();
    // DIAGNOSTIC: Allow compile-time override to LSB mode for waveform inspection
    #ifndef PARLIO_FORCE_LSB_MODE
        #define PARLIO_FORCE_LSB_MODE 0  // Default: use MSB (correct mode)
    #endif

    ParlioPeripheralConfig config(
        pins,  // Uses actual pin vector (size = dataWidth)
        mClockFreqHz,
        FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH,
        65534,
        #if PARLIO_FORCE_LSB_MODE
        ParlioBitPackOrder::FL_PARLIO_LSB,  // DIAGNOSTIC: LSB mode for waveform inspection
        #else
        ParlioBitPackOrder::FL_PARLIO_MSB,  // MSB packing required - Wave8 data is MSB-ordered
        #endif
        psram_cap_available  // prefer_psram only when DMA-capable PSRAM exists
    );
    FL_LOG_PARLIO("PARLIO_INIT: Config - clock=" << mClockFreqHz << " queue_depth=" << FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH);

    // DIAGNOSTIC: Log bit packing mode
    #if PARLIO_FORCE_LSB_MODE
        FL_LOG_PARLIO("PARLIO_INIT: ⚠️ LSB MODE ENABLED (DIAGNOSTIC) - expect waveform failures!");
    #else
        FL_LOG_PARLIO("PARLIO_INIT: MSB mode enabled (correct for Wave8)");
    #endif

    // Initialize peripheral
    FL_LOG_PARLIO("PARLIO_INIT: Calling mPeripheral->initialize()");
    if (!mPeripheral->initialize(config)) {
        FL_LOG_PARLIO("PARLIO_INIT: FAILED to initialize peripheral");
        mPeripheral = nullptr;
        return false;
    }
    FL_LOG_PARLIO("PARLIO_INIT: Peripheral initialized successfully");

    // Register ISR callback
    FL_LOG_PARLIO("PARLIO_INIT: Registering ISR callback");
    if (!mPeripheral->registerTxDoneCallback(
            reinterpret_cast<void*>(txDoneCallback), this)) { // ok reinterpret cast - callback function pointer to void*
        FL_LOG_PARLIO("PARLIO_INIT: FAILED to register ISR callback");
        FL_LOG_PARLIO("PARLIO: Failed to register callbacks");
        mPeripheral->deinitialize(); // Clean up TX unit so re-init can succeed
        mPeripheral = nullptr;
        return false;
    }
    FL_LOG_PARLIO("PARLIO_INIT: ISR callback registered successfully");

    // Calculate ring buffer capacity. Use the PSRAM cap only when a DMA-capable
    // PSRAM heap exists; C6 has no PSRAM and should go straight to the SRAM cap.
    ParlioBufferCalculator calc(mDataWidth, mUseWave3, mClockFreqHz);
    size_t raw_capacity = calc.calculateRingBufferCapacity(
        maxLedsPerChannel, mResetUs, ParlioRingBuffer3::RING_BUFFER_COUNT,
        psram_cap_available ? FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM
                             : FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES);

    // NEEDS REVIEW!!! are we potentially accessing memory outside of the cache??
    // Please invesatigate
    // Round up to 64-byte multiple to ensure cache line alignment
    // This addresses byte offset 318 error (2 bytes before cache boundary 320)
    // Cache sync requires both address AND size to be cache-aligned
    mRingBufferCapacity = ((raw_capacity + 63) / 64) * 64;

    if (mRingBufferCapacity != raw_capacity) {
        FL_LOG_PARLIO("PARLIO: Rounded buffer capacity from " << raw_capacity << " to " << mRingBufferCapacity << " bytes (64-byte alignment)");
    }

    // Allocate ring buffers - try PSRAM-sized first only when PSRAM exists.
    FL_LOG_PARLIO("PARLIO_INIT: Allocating ring buffers (capacity=" << mRingBufferCapacity << ")");
    if (!allocateRingBuffers()) {
        if (psram_cap_available) {
            FL_LOG_PARLIO("PARLIO_INIT: PSRAM-sized allocation failed, retrying with SRAM cap");
            raw_capacity = calc.calculateRingBufferCapacity(
                maxLedsPerChannel, mResetUs, ParlioRingBuffer3::RING_BUFFER_COUNT,
                FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES);
            mRingBufferCapacity = ((raw_capacity + 63) / 64) * 64;
        }

        if (!psram_cap_available || !allocateRingBuffers()) {
            FL_LOG_PARLIO("PARLIO_INIT: FAILED to allocate ring buffers");
            mPeripheral->deinitialize(); // Clean up TX unit so re-init can succeed
            mPeripheral = nullptr;
            return false;
        }
    }
    FL_LOG_PARLIO("PARLIO_INIT: Ring buffers allocated successfully (capacity=" << mRingBufferCapacity << ")");

    // Initialize ISR context state
    if (mIsrContext) {
        mIsrContext->mTransmitting.store(false, fl::memory_order_release);
        mIsrContext->mStreamComplete.store(false, fl::memory_order_release);
        mIsrContext->mCurrentByte = 0;
        mIsrContext->mTotalBytes = 0;
    }
    mErrorOccurred = false;

    mMaxLedsPerChannel = maxLedsPerChannel;
    mInitialized = true;
    FL_LOG_PARLIO("PARLIO_INIT: Initialization COMPLETE - mInitialized=true, maxLeds=" << maxLedsPerChannel);
    return true;
}

bool ParlioEngine::initializeSpi(const fl::vector<int>& pins,
                                  u32 spiClockHz,
                                  size_t maxBytesPerChannel) FL_NOEXCEPT {
    // SPI-over-PARLIO requires exactly 2 pins (clock + data)
    if (pins.size() != 2) {
        FL_WARN("PARLIO_SPI: Expected 2 pins (clock, data), got " << pins.size());
        return false;
    }

    // Detect config change: if already initialized with different mode or clock, deinit first
    bool needsReinit = false;
    if (mInitialized) {
        if (mEncodingMode != EncodingMode::SPI || mSpiClockHz != spiClockHz ||
            mDataWidth != 2 || mPins.size() != pins.size() ||
            (mPins.size() >= 2 && (mPins[0] != pins[0] || mPins[1] != pins[1]))) {
            needsReinit = true;
        } else if (mPeripheral && mPeripheral->isInitialized()) {
            // Same config, already initialized - reuse
            return true;
        }
    }

    if (needsReinit && mPeripheral && mPeripheral->isInitialized()) {
        if (mTxUnitEnabled) {
            mPeripheral->disable();
        }
        mPeripheral->deinitialize();
        mInitialized = false;
        mTxUnitEnabled = false;
    }

    // Set SPI-specific state before calling base initialize path
    mEncodingMode = EncodingMode::SPI;
    mSpiClockHz = spiClockHz;

    // Convert max bytes to max LEDs for buffer sizing (3 bytes per "LED")
    size_t maxLedsEquivalent = (maxBytesPerChannel + 2) / 3;
    if (maxLedsEquivalent == 0) maxLedsEquivalent = 1;

    // Store data width and pins directly (bypass full initialize which does Wave8 LUT)
    mDataWidth = 2;
    mPins = pins;
    mActualChannels = 2;
    mDummyLanes = 0;

    // Store timing parameters
    mTimingT1Ns = 0;
    mTimingT2Ns = 0;
    mTimingT3Ns = 0;
    mResetUs = 0;  // SPI protocols use explicit start/end frames, no reset

    // Get peripheral
    if (mPeripheral == nullptr) {
        mPeripheral = getParlioPeripheral();
    }

    // Allocate ISR context
    if (!mIsrContext) {
        mIsrContext = fl::make_unique<ParlioIsrContext>();
    }

    // PARLIO clock = 2× SPI clock (2 ticks per SPI bit: rise + fall)
    u32 parlioClockHz = spiClockHz * 2;

    ParlioPeripheralConfig config(
        pins,
        parlioClockHz,
        FL_ESP_PARLIO_HARDWARE_QUEUE_DEPTH,
        65534,
        ParlioBitPackOrder::FL_PARLIO_MSB,
        parlioDmaPsramAvailable()
    );

    if (!mPeripheral->initialize(config)) {
        FL_WARN("PARLIO_SPI: Peripheral initialization failed");
        mPeripheral = nullptr;
        return false;
    }

    // Register ISR callback
    if (!mPeripheral->registerTxDoneCallback(
            reinterpret_cast<void*>(txDoneCallback), this)) { // ok reinterpret cast
        FL_WARN("PARLIO_SPI: Failed to register ISR callback");
        mPeripheral->deinitialize();
        mPeripheral = nullptr;
        return false;
    }

    // Calculate ring buffer capacity (SPI expansion: 4:1)
    // Each input byte produces 4 DMA bytes
    ParlioBufferCalculator calc{mDataWidth};
    const bool psram_cap_available = parlioDmaPsramAvailable();
    size_t raw_capacity = calc.calculateRingBufferCapacity(
        maxLedsEquivalent, 0, ParlioRingBuffer3::RING_BUFFER_COUNT,
        psram_cap_available ? FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES_PSRAM
                             : FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES);
    mRingBufferCapacity = ((raw_capacity + 63) / 64) * 64;

    if (!allocateRingBuffers()) {
        if (psram_cap_available) {
            raw_capacity = calc.calculateRingBufferCapacity(
                maxLedsEquivalent, 0, ParlioRingBuffer3::RING_BUFFER_COUNT,
                FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES);
            mRingBufferCapacity = ((raw_capacity + 63) / 64) * 64;
        }

        if (!psram_cap_available || !allocateRingBuffers()) {
            FL_WARN("PARLIO_SPI: Failed to allocate ring buffers");
            mPeripheral->deinitialize();
            mPeripheral = nullptr;
            return false;
        }
    }

    // Initialize ISR context state
    if (mIsrContext) {
        mIsrContext->mTransmitting.store(false, fl::memory_order_release);
        mIsrContext->mStreamComplete.store(false, fl::memory_order_release);
        mIsrContext->mCurrentByte = 0;
        mIsrContext->mTotalBytes = 0;
    }
    mErrorOccurred = false;

    mInitialized = true;
    FL_LOG_PARLIO("PARLIO_SPI: Initialization COMPLETE - clockHz=" << parlioClockHz);
    return true;
}

// ============================================================================
// Static utility: Encode a single SPI byte into 4 DMA bytes
// ============================================================================
void ParlioEngine::encodeSpiByteForTest(u8 dataByte, u8 output[4]) FL_NOEXCEPT {
    // 2-bit PARLIO mode with MSB packing.
    // Each DMA byte holds 4 PARLIO ticks (2 bits per tick, MSB first).
    //
    // For MSB packing, the FIRST tick occupies the MSB position:
    //   byte = (tick0 << 6) | (tick1 << 4) | (tick2 << 2) | tick3
    //
    // Lane assignment: bit0 = lane0 (clock), bit1 = lane1 (data)
    //
    // For each SPI data bit (MSB first, bit 7 down to bit 0):
    //   tick_rise = (data_bit << 1) | 1    (clock=1, data=data_bit)
    //   tick_fall = (data_bit << 1) | 0    (clock=0, data=data_bit)
    //
    // 8 SPI bits -> 16 ticks -> 4 DMA bytes (4 ticks per byte)

    for (int outIdx = 0; outIdx < 4; outIdx++) {
        u8 byte_val = 0;

        for (int tickInByte = 0; tickInByte < 4; tickInByte++) {
            int tickNum = outIdx * 4 + tickInByte;
            int spiBitIdx = tickNum / 2;
            bool isFall = (tickNum % 2) == 1;

            u8 dataBit = (dataByte >> (7 - spiBitIdx)) & 1;

            u8 tickVal;
            if (isFall) {
                tickVal = (dataBit << 1) | 0;  // clock LOW
            } else {
                tickVal = (dataBit << 1) | 1;  // clock HIGH
            }

            int shift = (3 - tickInByte) * 2;
            byte_val |= (tickVal << shift);
        }

        output[outIdx] = byte_val;
    }
}

// ============================================================================
// HOT PATH - NO LOGGING IN THIS FUNCTION
// ============================================================================
FL_OPTIMIZE_FUNCTION bool FL_IRAM
ParlioEngine::populateDmaBufferSpi(
    u8* outputBuffer, size_t outputCapacity,
    size_t startByte, size_t byteCount,
    size_t& outputBytesWritten) FL_NOEXCEPT {

    size_t outputIdx = 0;

    // SPI encoding: single lane of data (lane 0 in scratch buffer)
    // The clock+data interleaving is generated by encodeSpiByteForTest()
    for (size_t byteOffset = 0; byteOffset < byteCount; byteOffset++) {
        if (outputIdx + 4 > outputCapacity) {
            outputBytesWritten = outputIdx;
            return false;
        }

        // Get source byte from scratch buffer (single lane, lane 0)
        u8 srcByte = mScratchBuffer[0 * mLaneStride + startByte + byteOffset];

        // Encode into 4 DMA bytes
        encodeSpiByteForTest(srcByte, outputBuffer + outputIdx);
        outputIdx += 4;
    }

    outputBytesWritten = outputIdx;
    return true;
}

bool ParlioEngine::beginTransmission(const u8* scratchBuffer,
                                     size_t totalBytes,
                                     size_t numLanes,
                                     size_t laneStride) FL_NOEXCEPT {
    FL_LOG_PARLIO("PARLIO_TX: beginTransmission() called - totalBytes=" << totalBytes << " numLanes=" << numLanes);

    if (!mInitialized || !mPeripheral || !mIsrContext) {
        FL_LOG_PARLIO("PARLIO_TX: FAILED - not initialized (mInit=" << mInitialized << " mPeripheral=" << (mPeripheral?1:0) << " mIsr=" << (mIsrContext?1:0) << ")");
        return false;
    }

    // CRITICAL: Wait for hardware TX queue to fully drain from any previous transmission.
    // The ESP-IDF PARLIO driver's internal DMA queue may still have the last buffer
    // queued even after our ISR context shows mStreamComplete=true. Without this,
    // consecutive transmissions with the same config fail silently (0 bytes output).
    if (mTxUnitEnabled) {
        mPeripheral->waitAllDone(100);  // 100ms timeout - should be near-instant
    }

    // Check if already transmitting
    if (mIsrContext->mTransmitting.load(fl::memory_order_acquire)) {
        FL_LOG_PARLIO("PARLIO_TX: FAILED - transmission already in progress");
        FL_LOG_PARLIO("PARLIO: Transmission already in progress");
        return false;
    }

    if (totalBytes == 0) {
        FL_LOG_PARLIO("PARLIO_TX: Zero bytes - returning success");
        return true; // Nothing to transmit
    }

    // ASYNC MODE: No task handle capture needed
    // Completion detected via poll() reading mStreamComplete flag

    // Store scratch buffer reference (NOT owned by this class)
    mScratchBuffer = scratchBuffer;

    // CRITICAL FIX: Validate and compute correct lane stride
    // The lane stride represents bytes per lane in the scratch buffer.
    // If caller incorrectly passes totalBytes as laneStride, compute the correct value.
    // The scratch buffer layout is: [lane0_data][lane1_data]...[laneN_data]
    // where each lane has (totalBytes / numLanes) bytes.
    size_t computed_lane_stride = totalBytes / numLanes;

    // Use the smaller of provided and computed stride to prevent buffer overflow
    // If laneStride is correct (= computed), this is a no-op
    // If laneStride is incorrect (= totalBytes), this fixes it
    if (laneStride > computed_lane_stride) {
        FL_LOG_PARLIO("PARLIO: Correcting laneStride from " << laneStride << " to " << computed_lane_stride);
        mLaneStride = computed_lane_stride;
    } else {
        mLaneStride = laneStride;
    }

    // Initialize IsrContext state for ring buffer streaming
    // CRITICAL FIX (Iteration 3): mTotalBytes represents bytes-per-lane, not total bytes
    // The buffer access pattern uses: mScratchBuffer[lane * mLaneStride + startByte + byteOffset]
    // For this to work correctly, startByte+byteOffset must stay within laneStride.
    // Example: 4 lanes, 15 bytes/lane, 60 total bytes
    //   - mTotalBytes should be 15 (per lane), loop iterates byte positions 0-14
    //   - Each iteration processes lane[i][bytePos] for all lanes simultaneously
    //   - lane 0 accesses [0-14], lane 1 accesses [15-29], etc.
    // CRITICAL: Use the corrected mLaneStride, NOT the original laneStride parameter
    mIsrContext->mTotalBytes = mLaneStride;
    mIsrContext->mNumLanes = numLanes;
    mIsrContext->mCurrentByte = 0;
    mIsrContext->mStreamComplete.store(false, fl::memory_order_release);
    mErrorOccurred = false;
    mIsrContext->mTransmitting.store(false, fl::memory_order_release);

    // Initialize ring buffer indices and count
    mIsrContext->mRingReadIdx = 0;
    mIsrContext->mRingWriteIdx = 0;
    mIsrContext->mRingCount = 0;
    mIsrContext->mRingError.store(false, fl::memory_order_release);
    mIsrContext->mHardwareIdle.store(false, fl::memory_order_release);
    mIsrContext->mUnderrunCount = 0;
    mIsrContext->mNextByteOffset = 0;

    // Initialize counters
    mIsrContext->mIsrCount = 0;
    mIsrContext->mBytesTransmitted = 0;
    mIsrContext->mChunksCompleted = 0;
    mIsrContext->mTransmissionActive.store(true, fl::memory_order_release);
    mIsrContext->mEndTimeUs = 0;

    // Initialize debug counters
    mIsrContext->mDebugTxDoneCount = 0;
    mIsrContext->mDebugWorkerIsrCount = 0;
    mIsrContext->mDebugLastTxDoneTime = 0;
    mIsrContext->mDebugLastWorkerIsrTime = 0;

    // Pre-populate ring buffers (fill all buffers if possible)
    FL_LOG_PARLIO("PARLIO_TX: Pre-populating ring buffers - mScratchBuffer=" << (void*)mScratchBuffer << " stride=" << mLaneStride);
    while (hasRingSpace() && populateNextDMABuffer()) {
        // Buffer populated into ring
    }

    // Get actual number of buffers populated
    size_t buffers_populated = mIsrContext->mRingCount;
    FL_LOG_PARLIO("PARLIO_TX: Ring buffers populated - count=" << buffers_populated);

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

    // CRITICAL: Always disable and re-enable the PARLIO peripheral between transmissions.
    // The ESP32 PARLIO hardware requires a disable/enable cycle to reset its DMA state
    // machine. Without this, consecutive transmissions with the same config silently fail
    // (DMA doesn't start, TX produces no output, RX captures 0 bytes).
    // Measured cost: ~25-30 µs (negligible).
    if (mTxUnitEnabled) {
        FL_LOG_PARLIO("PARLIO_TX: Disabling peripheral for re-enable cycle");
        mPeripheral->disable();
        mTxUnitEnabled = false;
    }

    FL_LOG_PARLIO("PARLIO_TX: Enabling peripheral");
    if (!mPeripheral->enable()) {
        FL_LOG_PARLIO("PARLIO: Failed to enable peripheral");
        mErrorOccurred = true;
        return false;
    }
    mTxUnitEnabled = true;
    FL_LOG_PARLIO("PARLIO_TX: Peripheral enabled successfully");

    // Queue first buffer to start transmission
    FL_LOG_PARLIO("PARLIO: Starting ISR-based streaming | first_buffer_size="
           << mRingBuffer->sizes[0] << " | buffers_ready=" << buffers_populated);

    size_t first_buffer_size = mRingBuffer->sizes[0];
    FL_LOG_PARLIO("PARLIO_TX: Starting transmission - first_buffer=" << first_buffer_size << " bytes, buffers_ready=" << buffers_populated);

    // CRITICAL FIX: Mark transmission started BEFORE submitting buffer
    // This closes the race window where txDoneCallback could fire before flag is set (Issue #2)
    mIsrContext->mTransmitting.store(true, fl::memory_order_release);

    // CRITICAL FIX (Iteration 2): Advance read index BEFORE submitting first buffer
    // This prevents race condition where txDone fires before index is advanced
    // Root cause: If txDone fires with read_idx=0, it calculates wrong completed_buffer_idx
    mIsrContext->mRingReadIdx = 1;
    mIsrContext->mRingCount = buffers_populated - 1;
    FL_MEMORY_BARRIER;

    if (!mPeripheral->transmit(mRingBuffer->ptrs[0], first_buffer_size * 8, 0x0000)) {
        FL_LOG_PARLIO("PARLIO: Failed to queue first buffer");
        mIsrContext->mTransmitting.store(false, fl::memory_order_release);
        mIsrContext->mRingReadIdx = 0;  // Rollback index on error
        mIsrContext->mRingCount = buffers_populated;  // Rollback count on error
        mErrorOccurred = true;
        return false;
    }
    FL_LOG_PARLIO("PARLIO_TX: First buffer submitted successfully - transmission started");

    // #2548: Eager-queue additional pre-encoded buffers to the IDF TX queue,
    // so the hardware chains them via DMA descriptors instead of waiting for
    // an ISR-latency gap between txDone and next-submit. This is what makes
    // encode/TX interleaving actually work — the TX of buffer 0 runs in the
    // background while show() submits buffer 1+ (and any FastLED bookkeeping
    // runs concurrently with TX too).
    //
    // CRITICAL: only eager-queue when the entire frame is already encoded
    // (i.e. the frame fit in the engine's 3-buffer ring). For frames larger
    // than the ring, encoding must continue in the background via the
    // txDoneCallback → workerIsrCallback streaming path — those need
    // `mRingCount` to track "buffers waiting to be submitted to IDF", which
    // gets corrupted if we drain it eagerly. The "ring empty but more data
    // pending" branch in txDoneCallback would also incorrectly set
    // `mHardwareIdle = true` while the IDF queue is still draining.
    //
    // Configurable via FL_PARLIO_EAGER_QUEUE (default ON). See header comment
    // for measured impact and disable instructions.
#if FL_PARLIO_EAGER_QUEUE
    const bool frame_fits_in_ring =
        (mIsrContext->mNextByteOffset >= mIsrContext->mTotalBytes);
    if (frame_fits_in_ring) {
        while (mIsrContext->mRingCount > 0) {
            size_t idx = mIsrContext->mRingReadIdx;
            size_t sz = mRingBuffer->sizes[idx];
            if (!mPeripheral->transmit(mRingBuffer->ptrs[idx], sz * 8, 0x0000)) {
                // Best-effort: stop on first failure. txDoneCallback will resubmit
                // remaining buffers from the engine ring as TX progresses.
                break;
            }
            mIsrContext->mRingReadIdx = (mIsrContext->mRingReadIdx + 1) % ParlioRingBuffer3::RING_BUFFER_COUNT;
            mIsrContext->mRingCount = mIsrContext->mRingCount - 1;
            FL_MEMORY_BARRIER;
        }
    }
    // else: frame > ring capacity → leave remaining buffers in the engine
    // ring; the existing chunked-streaming path (txDoneCallback +
    // workerIsrCallback) will submit them one at a time as TX drains, while
    // also encoding new chunks on the fly.
#endif

    //=========================================================================
    // Worker function now called directly from txDoneCallback (one-shot pattern)
    //=========================================================================
    // Refactored from periodic timer ISR to direct invocation:
    // - Eliminates 50µs periodic timer overhead
    // - Worker function called only when buffer population is needed
    // - txDoneCallback directly invokes workerIsrCallback when ring has space
    // - Simpler code path with lower latency
    //=========================================================================

    FL_LOG_PARLIO("PARLIO: Worker function configured (one-shot mode, called from txDoneCallback)"
           << " | buffers_ready=" << buffers_populated);

    // For test/mock environments, block and wait for completion
    // Real hardware uses async mode where caller polls via poll() method
    #ifdef FASTLED_STUB_IMPL
    // Block until transmission completes by polling
    FL_LOG_PARLIO("PARLIO STUB: Entering polling loop - totalBytes=" << totalBytes
                   << " streamComplete=" << mIsrContext->mStreamComplete.load(fl::memory_order_acquire));
    size_t poll_iterations = 0;
    while (!mIsrContext->mStreamComplete.load(fl::memory_order_acquire) && !mErrorOccurred) {
        ParlioEngineState state = poll();
        poll_iterations++;

        if (poll_iterations % 100 == 0) {
            FL_LOG_PARLIO("PARLIO STUB: Poll iteration " << poll_iterations
                   << " - bytesTransmitted=" << mIsrContext->mBytesTransmitted
                   << "/" << mIsrContext->mTotalBytes
                   << " streamComplete=" << mIsrContext->mStreamComplete.load(fl::memory_order_acquire)
                   << " transmitting=" << mIsrContext->mTransmitting.load(fl::memory_order_acquire)
                   << " ringCount=" << mIsrContext->mRingCount
                   << " txDone=" << mIsrContext->mDebugTxDoneCount
                   << " worker=" << mIsrContext->mDebugWorkerIsrCount);
        }

        if (state == ParlioEngineState::READY || state == ParlioEngineState::ERROR) {
            break;
        }
        // Small delay to avoid busy-wait
        mPeripheral->delay(1);
    }
    FL_LOG_PARLIO("PARLIO STUB: Exited polling loop after " << poll_iterations << " iterations");

    if (mErrorOccurred) {
        FL_LOG_PARLIO("PARLIO: Error occurred during transmission");
        return false;
    }
    #endif

#ifdef FL_DEBUG
    // Debug task: Logs ISR state every 500ms for diagnostics
    // Iteration 7 confirmed: Debug task is NOT the cause of crash
    if (!mDebugTask.is_valid()) {
        // Create debug task using unified task API (wraps TaskCoroutine)
        // Lambda captures 'this' to access ParlioEngine instance
        CoroutineConfig config;
        config.func = [this]() { debugTaskFunction(this); };
        config.name = "parlio_debug";
        config.stack_size = 4096;  // 4KB stack (increased for ESP32-C6 RISC-V - stack overflow at 2KB)
        config.priority = 1;       // Low priority (tskIDLE_PRIORITY + 1)
        mDebugTask = fl::task::coroutine(config);

        FL_LOG_PARLIO("PARLIO: Debug task created (500ms logging interval)");
    }
#endif

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

ParlioEngineState ParlioEngine::poll() FL_NOEXCEPT {
    if (!mInitialized || !mPeripheral || !mIsrContext) {
        return ParlioEngineState::READY;
    }

    // Check for errors
    if (mErrorOccurred) {
        FL_LOG_PARLIO("PARLIO: Error occurred during transmission");
        mIsrContext->mTransmitting.store(false, fl::memory_order_release);
        mErrorOccurred = false;
        return ParlioEngineState::ERROR;
    }

    // Check if streaming is complete
    if (mIsrContext->mStreamComplete.load(fl::memory_order_acquire)) {
        // Wait for final chunk to complete (non-blocking poll)
        if (mPeripheral->waitAllDone(0)) {
            // Clear completion flags only once the peripheral is truly idle.
            mIsrContext->mTransmitting.store(false, fl::memory_order_release);
            mIsrContext->mStreamComplete.store(false, fl::memory_order_release);

            // All transmissions complete - disable peripheral (only if currently enabled)
            if (mTxUnitEnabled) {
                if (!mPeripheral->disable()) {
                    FL_LOG_PARLIO("PARLIO: Failed to disable peripheral");
                } else {
                    mTxUnitEnabled = false;
                }
            }

            // Short delay for GPIO stabilization
            mPeripheral->delayMicroseconds(100);

            return ParlioEngineState::READY;
        } else {
            // Still draining final transmission
            return ParlioEngineState::DRAINING;
        }
    }

    // If not transmitting, we're ready
    if (!mIsrContext->mTransmitting.load(fl::memory_order_acquire)) {
        return ParlioEngineState::READY;
    }

    // Incremental ring buffer refill during transmission
    while (hasRingSpace() && populateNextDMABuffer()) {
        // Continue populating buffers
    }

    // Transmission in progress (ISR-driven, async)
    return ParlioEngineState::DRAINING;
}

bool ParlioEngine::isTransmitting() const FL_NOEXCEPT {
    if (!mIsrContext) {
        return false;
    }
    return mIsrContext->mTransmitting.load(fl::memory_order_acquire);
}

ParlioDebugMetrics ParlioEngine::getDebugMetrics() const FL_NOEXCEPT {
    ParlioDebugMetrics metrics = {};

    if (!mIsrContext) {
        return metrics;
    }

    (void)mIsrContext->mStreamComplete.load(fl::memory_order_acquire);
    (void)mIsrContext->mTransmitting.load(fl::memory_order_acquire);

    metrics.mStartTimeUs = 0; // Not tracked yet
    metrics.mEndTimeUs = mIsrContext->mEndTimeUs;
    metrics.mIsrCount = mIsrContext->mIsrCount;
    metrics.mChunksQueued = 0; // Not tracked yet
    metrics.mChunksCompleted = mIsrContext->mChunksCompleted;
    metrics.mBytesTotal = mIsrContext->mTotalBytes;
    metrics.mBytesTransmitted = mIsrContext->mBytesTransmitted;
    metrics.mTxDoneCount = mIsrContext->mDebugTxDoneCount;
    metrics.mWorkerIsrCount = mIsrContext->mDebugWorkerIsrCount;
    metrics.mUnderrunCount = mIsrContext->mUnderrunCount;
    metrics.mRingCount = mIsrContext->mRingCount;
    metrics.mErrorCode = mErrorOccurred ? 1 : 0;
    metrics.mRingError = mIsrContext->mRingError.load(fl::memory_order_acquire);
    metrics.mHardwareIdle = mIsrContext->mHardwareIdle.load(fl::memory_order_acquire);
    metrics.mTransmissionActive =
        mIsrContext->mTransmissionActive.load(fl::memory_order_acquire);

    return metrics;
}

} // namespace detail
} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END

#endif // (ESP32 && FASTLED_ESP32_HAS_PARLIO) || FASTLED_STUB_IMPL
