/// @file parlio_engine.cpp
/// @brief PARLIO hardware abstraction layer (HAL) implementation for ESP32
///
/// This file contains the low-level PARLIO hardware management code extracted
/// from channel_engine_parlio.cpp. It handles all hardware-specific operations
/// including ISR callbacks, DMA buffer generation, and ring buffer streaming.

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "parlio_engine.h"
#include "fl/delay.h"
#include "fl/error.h"
#include "fl/log.h"
#include "fl/transposition.h"
#include "fl/warn.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/assert.h"
#include "fl/stl/time.h"
#include "platforms/esp/32/core/fastpin_esp32.h" // For _FL_VALID_PIN_MASK

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/parlio_tx.h"
#include "esp_heap_caps.h" // For DMA-capable memory allocation
#include "esp_intr_alloc.h" // For software interrupt allocation
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" // For semaphore operations
#include "soc/soc_caps.h" // For SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH
FL_EXTERN_C_END

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
static constexpr uint32_t PARLIO_CLOCK_FREQ_HZ = 8000000; // 8.0 MHz

constexpr size_t MAX_LEDS_PER_CHANNEL = 300; // Support up to 300 LEDs per channel (configurable)

// ESP32-C6 PARLIO hardware transaction queue depth limit
// CRITICAL: This value CANNOT be changed - it is a hardware limitation
// The ESP32-C6 PARLIO peripheral has a 3-state FSM (READY/PROGRESS/COMPLETE)
// Setting trans_queue_depth > 3 causes queue desynchronization and system crashes
// Empirical testing results (2025-12-29):
//   - trans_queue_depth = 3: Stable operation (hardware maximum)
//   - trans_queue_depth = 4: Queue desync, 64% more underruns
//   - trans_queue_depth = 8: Watchdog timeout crash
static constexpr size_t PARLIO_HARDWARE_QUEUE_DEPTH = 3;

//=============================================================================
// ISR-Safe Memory Operations
//=============================================================================

/// @brief ISR-safe memset replacement (manual loop copy)
/// fl::memset is not allowed in ISR context on some platforms
/// This function uses a simple loop to zero memory
FL_OPTIMIZE_FUNCTION static inline void FL_IRAM
isr_memset_zero(uint8_t* dest, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = 0x00;
    }
}

//=============================================================================
// Cross-Platform Memory Barriers
//=============================================================================
// Memory barriers ensure correct synchronization between ISR and main thread
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields, then executes barrier before reading
// non-volatile fields
// - Barrier ensures all ISR writes are visible to main thread
//
// Platform-Specific Barriers:
// - Xtensa (ESP32, ESP32-S3): memw instruction (memory write barrier)
// - RISC-V (ESP32-C6, ESP32-C3, ESP32-H2): fence rw,rw instruction (full memory
// fence)
//
#if defined(__XTENSA__)
              // Xtensa architecture (ESP32, ESP32-S3)
#define PARLIO_MEMORY_BARRIER() asm volatile("memw" ::: "memory")
#elif defined(__riscv)
              // RISC-V architecture (ESP32-C6, ESP32-C3, ESP32-H2, ESP32-P4)
#define PARLIO_MEMORY_BARRIER() asm volatile("fence rw, rw" ::: "memory")
#else
#error \
    "Unsupported architecture for PARLIO memory barrier (expected __XTENSA__ or __riscv)"
#endif

//=============================================================================
// ParlioIsrContext Implementation
//=============================================================================

ParlioIsrContext::ParlioIsrContext()
    : mStreamComplete(false), mTransmitting(false), mCurrentByte(0),
      mRingReadIdx(0), mRingWriteIdx(0), mRingCount(0), mRingError(false),
      mHardwareIdle(false), mNextByteOffset(0), mWorkerIsrEnabled(false),
      mTotalBytes(0), mNumLanes(0), mIsrCount(0),
      mBytesTransmitted(0), mChunksCompleted(0),
      mTransmissionActive(false), mEndTimeUs(0) {
}

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
// Buffer Size Calculator - Unified DMA Buffer Size Calculations
//=============================================================================

/// @brief Unified calculator for PARLIO buffer sizes
///
/// Consolidates all buffer size calculations into a single, tested utility.
/// Wave8 expands each input byte to 64 pulses (8 bits × 8 pulses per bit).
/// Transposition packs pulses into bytes based on data_width.
struct ParlioBufferCalculator {
    size_t mDataWidth;

    /// @brief Calculate output bytes per input byte after wave8 + transpose
    /// @return Output bytes per input byte (8 for width≤8, 128 for width=16)
    size_t outputBytesPerInputByte() const {
        if (mDataWidth <= 8) {
            // Bit-packed: 64 pulses packed into (8 / mDataWidth) ticks per byte
            // For data_width=1: 64 pulses / 8 ticks = 8 bytes
            // For data_width=2: 64 pulses / 4 ticks = 16 bytes
            // For data_width=4: 64 pulses / 2 ticks = 32 bytes
            // For data_width=8: 64 pulses / 1 tick = 64 bytes
            size_t ticksPerByte = 8 / mDataWidth;
            return (64 + ticksPerByte - 1) / ticksPerByte;
        } else if (mDataWidth == 16) {
            // 16-bit mode: 64 pulses × 2 bytes per pulse = 128 bytes
            return 128;
        }
        return 8; // Fallback
    }

    /// @brief Calculate DMA buffer size for given input bytes
    /// @param inputBytes Number of input bytes to transmit
    /// @return Total DMA buffer size in bytes
    size_t dmaBufferSize(size_t inputBytes) const {
        return inputBytes * outputBytesPerInputByte();
    }

    /// @brief Calculate transpose output block size for populateDmaBuffer
    /// @return Block size in bytes for transpose operation
    size_t transposeBlockSize() const {
        if (mDataWidth <= 8) {
            size_t ticksPerByte = 8 / mDataWidth;
            size_t pulsesPerByte = 64;
            return (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;
        } else if (mDataWidth == 16) {
            return 128; // 64 pulses × 2 bytes per pulse
        }
        return 8; // Fallback
    }

    /// @brief Calculate optimal ring buffer capacity based on LED frame boundaries
    /// @param maxLedsPerChannel Maximum LEDs per channel (strip size)
    /// @param numRingBuffers Number of ring buffers (default: 3)
    /// @return DMA buffer capacity in bytes, aligned to LED boundaries
    ///
    /// Algorithm:
    /// 1. Calculate LEDs per buffer: maxLedsPerChannel / numRingBuffers
    /// 2. Convert to input bytes: LEDs × 3 bytes/LED × mDataWidth (multi-lane)
    /// 3. Apply wave8 expansion (8:1 ratio): input_bytes × outputBytesPerInputByte()
    /// 4. Add safety margin for boundary checks
    /// 5. Result is DMA buffer capacity per ring buffer
    ///
    /// Example (3000 LEDs, 1 lane, 3 ring buffers):
    /// - LEDs per buffer: 3000 / 3 = 1000 LEDs
    /// - Input bytes per buffer: 1000 × 3 × 1 = 3000 bytes
    /// - DMA bytes per buffer: 3000 × 8 = 24000 bytes
    /// - With safety margin: 24000 + 128 = 24128 bytes
    size_t calculateRingBufferCapacity(size_t maxLedsPerChannel, size_t numRingBuffers = 3) const {
        // Step 1: Calculate LEDs per buffer (divide total LEDs by number of buffers)
        size_t ledsPerBuffer = (maxLedsPerChannel + numRingBuffers - 1) / numRingBuffers;

        // Step 2: Calculate input bytes per buffer
        // - 3 bytes per LED (RGB)
        // - Multiply by mDataWidth for multi-lane (each lane gets same LED count)
        size_t inputBytesPerBuffer = ledsPerBuffer * 3 * mDataWidth;

        // Step 3: Apply wave8 expansion (8:1 ratio for ≤8-bit width, 128:1 for 16-bit)
        size_t dmaBufferCapacity = dmaBufferSize(inputBytesPerBuffer);

        // Step 4: Add safety margin to prevent boundary check failures
        // The populateDmaBuffer() boundary check at line 505 tests (outputIdx + blockSize > capacity)
        // When buffer is filled exactly to capacity, we need extra space for the final block
        // Safety margin = max(transposeBlockSize) = 128 bytes (for 16-bit mode)
        size_t safetyMargin = 128;
        dmaBufferCapacity += safetyMargin;

        return dmaBufferCapacity;
    }
};

//=============================================================================
// ParlioEngine - Singleton Implementation
//=============================================================================

ParlioEngine::ParlioEngine()
    : mInitialized(false),
      mDataWidth(0),
      mActualChannels(0),
      mDummyLanes(0),
      mTxUnit(nullptr),
      mTimingT1Ns(0),
      mTimingT2Ns(0),
      mTimingT3Ns(0),
      mIsrContext(nullptr),
      mMainTaskHandle(nullptr),
      mWorkerTaskHandle(nullptr),
      mRingBufferCapacity(0),
      mScratchBuffer(nullptr),
      mLaneStride(0),
      mWaveformExpansionBufferSize(0),
      mErrorOccurred(false) {
}

ParlioEngine::~ParlioEngine() {
    // Wait for any active transmissions to complete
    while (isTransmitting()) {
        fl::delayMicroseconds(100);
    }

    // Clean up PARLIO TX unit resources
    if (mTxUnit != nullptr) {
        // Wait for any pending transmissions (with timeout)
        esp_err_t err =
            parlio_tx_unit_wait_all_done(mTxUnit, pdMS_TO_TICKS(1000));
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Wait for transmission timeout during cleanup: "
                    << err);
        }

        // Disable TX unit
        err = parlio_tx_unit_disable(mTxUnit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit: " << err);
        }

        // Delete TX unit
        err = parlio_del_tx_unit(mTxUnit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit: " << err);
        }

        mTxUnit = nullptr;
    }

    // DMA buffers and waveform expansion buffer are automatically freed
    // by fl::unique_ptr destructors (RAII)

    // Clean up worker task
    if (mWorkerTaskHandle && mIsrContext) {
        // Disarm worker task (signals task to exit loop)
        mIsrContext->mWorkerIsrEnabled = false;
        PARLIO_MEMORY_BARRIER();

        // Wake up task if it's blocked waiting for notification
        xTaskNotifyGive(static_cast<TaskHandle_t>(mWorkerTaskHandle));

        // Give task time to exit gracefully (10ms should be plenty)
        vTaskDelay(pdMS_TO_TICKS(10));

        // Task will delete itself via vTaskDelete(NULL) in workerTaskFunction
        mWorkerTaskHandle = nullptr;
    }

    // Clean up IsrContext
    if (mIsrContext) {
        delete mIsrContext;
        mIsrContext = nullptr;
    }

    // Clear state
    mPins.clear();
}

ParlioEngine& ParlioEngine::getInstance() {
    static ParlioEngine instance;
    return instance;
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
ParlioEngine::txDoneCallback(parlio_tx_unit_handle_t tx_unit,
                              const void *edata, void *user_ctx) {
    // ⚠️  ISR CONTEXT - NO LOGGING ALLOWED - SEE FUNCTION HEADER ⚠️
    (void)edata;

    auto *self = static_cast<ParlioEngine *>(user_ctx);
    if (!self || !self->mIsrContext) {
        return false;
    }

    // Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext *ctx = self->mIsrContext;

    // Increment ISR call counter
    ctx->mIsrCount++;

    // Account for bytes from the buffer that just completed transmission
    // The buffer that completed is the one BEFORE the current read_idx
    // (CPU or previous ISR call advanced read_idx after submitting)
    volatile size_t read_idx = ctx->mRingReadIdx;
    size_t completed_buffer_idx = (read_idx + ParlioEngine::RING_BUFFER_COUNT - 1) % ParlioEngine::RING_BUFFER_COUNT;

    // Track transmitted bytes (using input byte count, not expanded DMA bytes)
    // Calculate input bytes from DMA buffer size
    ParlioBufferCalculator calc{self->mDataWidth};
    size_t dma_bytes = self->mRingBufferSizes[completed_buffer_idx];
    size_t input_bytes = dma_bytes / calc.outputBytesPerInputByte();
    ctx->mBytesTransmitted += input_bytes;
    ctx->mCurrentByte += input_bytes;
    ctx->mChunksCompleted++;

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

            // DISARM WORKER TASK on last transmission
            ctx->mWorkerIsrEnabled = false;
            PARLIO_MEMORY_BARRIER();

            BaseType_t higherPriorityTaskWoken = pdFALSE;

            // Wake up worker task so it can exit
            if (self->mWorkerTaskHandle) {
                vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(self->mWorkerTaskHandle),
                                      &higherPriorityTaskWoken);
            }

            // Signal main task that transmission is complete
            if (self->mMainTaskHandle) {
                vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(self->mMainTaskHandle),
                                      &higherPriorityTaskWoken);
            }

            return higherPriorityTaskWoken == pdTRUE;
        }

        // Ring empty but more data pending - mark hardware as idle so worker can restart
        ctx->mHardwareIdle = true; // Signal that hardware needs restart
        ctx->mTransmitting = false; // Hardware is idle, not transmitting
        return false;
    }

    // Next buffer is ready - submit it to hardware
    size_t buffer_idx = read_idx;
    uint8_t *buffer_ptr = self->mRingBufferPtrs[buffer_idx];  // Use cached pointer (ISR optimization)
    size_t buffer_size = self->mRingBufferSizes[buffer_idx];

    // Invalid buffer - set error flag
    if (!buffer_ptr || buffer_size == 0) {
        ctx->mRingError = true;
        return false;
    }

    // Submit buffer to hardware
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000; // Keep pins LOW between chunks

    esp_err_t err = parlio_tx_unit_transmit(tx_unit, buffer_ptr,
                                            buffer_size * 8, &tx_config);

    if (err == ESP_OK) {
        // Successfully submitted - advance read index (modulo-3) and decrement count
        ctx->mRingReadIdx = (ctx->mRingReadIdx + 1) % ParlioEngine::RING_BUFFER_COUNT;
        ctx->mRingCount = ctx->mRingCount - 1;
        ctx->mHardwareIdle = false; // Hardware is active again

        // ARM WORKER TASK if buffers need refilling
        if (ctx->mRingCount < ParlioEngine::RING_BUFFER_COUNT &&
            ctx->mNextByteOffset < ctx->mTotalBytes &&
            self->mWorkerTaskHandle) {

            BaseType_t higherPriorityTaskWoken = pdFALSE;
            // Notify worker task to populate more buffers
            vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(self->mWorkerTaskHandle),
                                  &higherPriorityTaskWoken);
            return higherPriorityTaskWoken == pdTRUE;
        }

        return false;
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
// Worker Task - Background DMA Buffer Population
//=============================================================================

// ============================================================================
// ⚠️ ⚠️ ⚠️  CRITICAL TASK SAFETY RULES - READ BEFORE MODIFYING ⚠️ ⚠️ ⚠️
// ============================================================================
//
// This function runs as a HIGH-PRIORITY FREERTOS TASK with strict constraints:
//
// 1. ❌ NO LOGGING (FL_LOG_PARLIO, FL_WARN, FL_ERROR, printf, etc.) - interferes with timing
// 2. ❌ NO BLOCKING OPERATIONS (mutex, delay, heap allocation inside loop, etc.)
// 3. ✅ MINIMIZE EXECUTION TIME (early exit if no work available)
// 4. ✅ Priority: configMAX_PRIORITIES - 1 (highest user priority, below ISRs)
//
// This task populates DMA buffers in the background while txDoneCallback
// submits them to hardware. The two coordinate via ring buffer count.
//
// FreeRTOS Task Notification Pattern:
// - Waits on ulTaskNotifyTake() for notification from txDoneCallback ISR
// - txDoneCallback calls vTaskNotifyGiveFromISR() when buffers need refilling
// - Task exits when mWorkerIsrEnabled becomes false (set by destructor)
//
// ============================================================================
FL_OPTIMIZE_FUNCTION void
ParlioEngine::workerTaskFunction(void* arg) {
    // ⚠️  HIGH-PRIORITY TASK CONTEXT - NO LOGGING ALLOWED - SEE FUNCTION HEADER ⚠️

    auto *self = static_cast<ParlioEngine *>(arg);
    if (!self || !self->mIsrContext) {
        vTaskDelete(NULL);  // Exit task if invalid context
        return;
    }

    ParlioIsrContext *ctx = self->mIsrContext;

    // Main worker loop - runs until disabled by destructor or completion
    while (true) {
        // Block until notified by txDoneCallback ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // CRITICAL: Early exit checks (in order of likelihood)

        // Check 1: Worker task disabled by on-done callback or destructor
        if (!ctx->mWorkerIsrEnabled) {
            break;  // Exit loop - task is being shut down
        }

        // Check 2: Ring buffer full (no space to populate)
        if (ctx->mRingCount >= ParlioEngine::RING_BUFFER_COUNT) {
            continue;  // Wait for next notification
        }

        // Check 3: All data already processed
        if (ctx->mNextByteOffset >= ctx->mTotalBytes) {
            continue;  // Wait for next notification (or shutdown)
        }

        // Work available - populate one complete buffer
        // Get next ring buffer index (0-2)
        size_t ring_index = ctx->mRingWriteIdx;

        // Get ring buffer pointer (use cached pointer for optimization)
        uint8_t *outputBuffer = self->mRingBufferPtrs[ring_index];
        if (!outputBuffer) {
            continue;  // Invalid buffer - wait for next notification
        }

        // Calculate byte range for this buffer
        size_t bytes_remaining = ctx->mTotalBytes - ctx->mNextByteOffset;
        size_t bytes_per_buffer = (ctx->mTotalBytes + ParlioEngine::RING_BUFFER_COUNT - 1) / ParlioEngine::RING_BUFFER_COUNT;

        // LED boundary alignment constant
        size_t bytes_per_led_all_lanes = 3 * ctx->mNumLanes;

        // CAP bytes_per_buffer at ring buffer capacity
        ParlioBufferCalculator calc{self->mDataWidth};
        size_t max_input_bytes_per_buffer = self->mRingBufferCapacity / calc.outputBytesPerInputByte();

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
        bool is_last_buffer = (buffers_already_populated >= ParlioEngine::RING_BUFFER_COUNT - 1) ||
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
        isr_memset_zero(outputBuffer, self->mRingBufferCapacity);

        // Generate waveform data
        size_t outputBytesWritten = 0;
        if (!self->populateDmaBuffer(outputBuffer, self->mRingBufferCapacity,
                                      ctx->mNextByteOffset, byte_count, outputBytesWritten)) {
            continue;  // Buffer overflow - skip and wait for next notification
        }

        // Store actual size of this buffer
        self->mRingBufferSizes[ring_index] = outputBytesWritten;

        // Update state for next buffer
        ctx->mNextByteOffset += byte_count;
        ctx->mRingWriteIdx = (ctx->mRingWriteIdx + 1) % ParlioEngine::RING_BUFFER_COUNT;
        ctx->mRingCount = ctx->mRingCount + 1;

        // Memory barrier to ensure state visible to on-done callback ISR
        PARLIO_MEMORY_BARRIER();
    }

    // Task is shutting down - cleanup and exit
    vTaskDelete(NULL);
}
// ============================================================================
// END OF WORKER TASK - Remember: NO LOGGING, NO BLOCKING, MINIMIZE EXECUTION TIME
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
    uint8_t* laneWaveforms = mWaveformExpansionBuffer.get();
    constexpr size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte

    size_t outputIdx = 0;
    size_t byteOffset = 0;

    // Two-stage architecture: Process one byte position at a time
    // Stage 1: Generate wave8bytes for ALL lanes → staging buffer
    // Stage 2: Transpose staging buffer → DMA output buffer

    // Use calculator for transpose block size
    ParlioBufferCalculator calc{mDataWidth};
    size_t blockSize = calc.transposeBlockSize();

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
            isr_memset_zero(firstDummyLane, dummyLaneBytes);
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

    // Ring has space if count is less than RING_BUFFER_COUNT
    return count < RING_BUFFER_COUNT;
}

bool ParlioEngine::allocateRingBuffers() {
    // One-time ring buffer allocation and initialization
    // Called once during initialize(), NOT per transmission
    // Buffers remain allocated, only POPULATED on-demand during transmission

    // Clear any existing ring buffers
    mRingBuffers.clear();
    mRingBufferPtrs.clear();  // Clear cached pointers
    mRingBufferSizes.clear();

    // Allocate all ring buffers with DMA capability
    for (size_t i = 0; i < RING_BUFFER_COUNT; i++) {
        fl::unique_ptr<uint8_t[], HeapCapsDeleter> buffer(
            static_cast<uint8_t *>(heap_caps_malloc(
                mRingBufferCapacity, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));

        if (!buffer) {
            FL_WARN("PARLIO: Failed to allocate ring buffer "
                    << i << "/" << RING_BUFFER_COUNT << " (requested "
                    << mRingBufferCapacity << " bytes)");
            // Clean up already allocated ring buffers (automatic via unique_ptr destructors)
            mRingBuffers.clear();
            mRingBufferSizes.clear();
            return false;
        }

        // Zero-initialize buffer to prevent garbage data
        fl::memset(buffer.get(), 0x00, mRingBufferCapacity);

        // Cache raw pointer before moving (optimization: avoid unique_ptr deref in hot paths)
        uint8_t* raw_ptr = buffer.get();
        mRingBuffers.push_back(fl::move(buffer));
        mRingBufferPtrs.push_back(raw_ptr);
        mRingBufferSizes.push_back(0); // Will be set during population
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
    uint8_t *outputBuffer = mRingBufferPtrs[ring_index];
    if (!outputBuffer) {
        FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        mErrorOccurred = true;
        return false;
    }

    // Calculate byte range for this buffer (divide total bytes into chunks)
    size_t bytes_remaining = mIsrContext->mTotalBytes - mIsrContext->mNextByteOffset;
    size_t bytes_per_buffer = (mIsrContext->mTotalBytes + RING_BUFFER_COUNT - 1) / RING_BUFFER_COUNT;

    // LED boundary alignment constant: 3 bytes (RGB) × lane count
    // Used for both capacity calculation and alignment rounding
    size_t bytes_per_led_all_lanes = 3 * mDataWidth;

    // CAP bytes_per_buffer at ring buffer capacity to enable streaming for large strips
    ParlioBufferCalculator calc{mDataWidth};
    size_t max_input_bytes_per_buffer = mRingBufferCapacity / calc.outputBytesPerInputByte();

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
    bool is_last_buffer = (buffers_already_populated >= RING_BUFFER_COUNT - 1) ||
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
    isr_memset_zero(outputBuffer, mRingBufferCapacity);

    // Generate waveform data using helper function
    size_t outputBytesWritten = 0;
    if (!populateDmaBuffer(outputBuffer, mRingBufferCapacity,
                          mIsrContext->mNextByteOffset, byte_count, outputBytesWritten)) {
        FL_WARN("PARLIO: Ring buffer overflow at ring_index=" << ring_index);
        mErrorOccurred = true;
        return false;
    }

    // Store actual size of this buffer
    mRingBufferSizes[ring_index] = outputBytesWritten;

    // Update state for next buffer (ISR context owns the state now)
    mIsrContext->mNextByteOffset += byte_count;
    mIsrContext->mRingWriteIdx = (mIsrContext->mRingWriteIdx + 1) % RING_BUFFER_COUNT;
    mIsrContext->mRingCount = mIsrContext->mRingCount + 1;

    // CRITICAL: Check if hardware went idle while we were populating
    if (mIsrContext->mHardwareIdle) {
        // Get the buffer that was just populated (read_idx points to next buffer to transmit)
        size_t buffer_idx = mIsrContext->mRingReadIdx;
        uint8_t *buffer_ptr = mRingBufferPtrs[buffer_idx];  // Use cached pointer for optimization
        size_t buffer_size = mRingBufferSizes[buffer_idx];

        if (buffer_ptr && buffer_size > 0) {
            // Submit buffer to hardware to restart transmission
            parlio_transmit_config_t tx_config = {};
            tx_config.idle_value = 0x0000;

            esp_err_t err = parlio_tx_unit_transmit(
                mTxUnit, buffer_ptr, buffer_size * 8, &tx_config);

            if (err == ESP_OK) {
                // Successfully restarted - advance read index and decrement count
                mIsrContext->mRingReadIdx = (mIsrContext->mRingReadIdx + 1) % RING_BUFFER_COUNT;
                mIsrContext->mRingCount = mIsrContext->mRingCount - 1;
                mIsrContext->mHardwareIdle = false;
                mIsrContext->mTransmitting = true;
            } else {
                FL_WARN("PARLIO CPU: Failed to restart hardware: " << err);
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

    // Validate data width
    if (dataWidth != 1 && dataWidth != 2 && dataWidth != 4 &&
        dataWidth != 8 && dataWidth != 16) {
        FL_WARN("PARLIO: Invalid data_width=" << dataWidth);
        return false;
    }

    // Allocate ISR context (cache-aligned, 64 bytes)
    if (!mIsrContext) {
        mIsrContext = new ParlioIsrContext();
    }

    // Validate pins
    if (pins.size() != dataWidth) {
        FL_WARN("PARLIO: Pin configuration error - expected "
                << dataWidth << " pins, got " << pins.size());
        return false;
    }

    for (size_t i = 0; i < pins.size(); i++) {
        if (!isParlioPinValid(pins[i])) {
            FL_WARN("PARLIO: Invalid pin " << pins[i] << " for channel " << i);
            return false;
        }
    }

    // Build wave8 expansion LUT from timing configuration
    ChipsetTiming chipsetTiming;
    chipsetTiming.T1 = mTimingT1Ns;
    chipsetTiming.T2 = mTimingT2Ns;
    chipsetTiming.T3 = mTimingT3Ns;
    chipsetTiming.RESET = 0;
    chipsetTiming.name = "PARLIO";

    mWave8Lut = buildWave8ExpansionLUT(chipsetTiming);

    // Configure PARLIO TX unit
    parlio_tx_unit_config_t config = {};
    config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    config.clk_in_gpio_num = static_cast<gpio_num_t>(-1);
    config.output_clk_freq_hz = PARLIO_CLOCK_FREQ_HZ;
    config.data_width = mDataWidth;
    config.trans_queue_depth = PARLIO_HARDWARE_QUEUE_DEPTH;
    config.max_transfer_size = 65534;
    config.bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB;
    config.sample_edge = PARLIO_SAMPLE_EDGE_POS;

    // Assign GPIO pins
    for (size_t i = 0; i < mDataWidth; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(mPins[i]);
    }
    for (size_t i = mDataWidth; i < 16; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(-1);
    }

    config.clk_out_gpio_num = static_cast<gpio_num_t>(-1);
    config.valid_gpio_num = static_cast<gpio_num_t>(-1);

    // Create TX unit
    esp_err_t err = parlio_new_tx_unit(&config, &mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to create TX unit: " << err);
        return false;
    }

    // Register ISR callback
    parlio_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done =
        reinterpret_cast<parlio_tx_done_callback_t>(txDoneCallback);

    err = parlio_tx_unit_register_event_callbacks(mTxUnit, &callbacks, this);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to register callbacks: " << err);
        parlio_del_tx_unit(mTxUnit);
        mTxUnit = nullptr;
        return false;
    }

    // Calculate ring buffer capacity
    ParlioBufferCalculator calc{mDataWidth};
    mRingBufferCapacity = calc.calculateRingBufferCapacity(maxLedsPerChannel, RING_BUFFER_COUNT);

    // Allocate ring buffers
    if (!allocateRingBuffers()) {
        FL_WARN("PARLIO: Failed to allocate ring buffers");
        parlio_del_tx_unit(mTxUnit);
        mTxUnit = nullptr;
        return false;
    }

    // Allocate waveform expansion buffer
    const size_t bytes_per_lane = sizeof(Wave8Byte); // 8 bytes per input byte
    const size_t waveform_buffer_size = mDataWidth * bytes_per_lane;

    mWaveformExpansionBuffer.reset(static_cast<uint8_t *>(
        heap_caps_malloc(waveform_buffer_size, MALLOC_CAP_INTERNAL)));

    if (!mWaveformExpansionBuffer) {
        FL_WARN("PARLIO: Failed to allocate waveform expansion buffer");
        parlio_del_tx_unit(mTxUnit);
        mTxUnit = nullptr;
        return false;
    }

    mWaveformExpansionBufferSize = waveform_buffer_size;

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
    if (!mInitialized || !mTxUnit || !mIsrContext) {
        FL_WARN("PARLIO: Cannot transmit - not initialized");
        return false;
    }

    // Check if already transmitting
    if (mIsrContext->mTransmitting) {
        FL_WARN("PARLIO: Transmission already in progress");
        return false;
    }

    if (totalBytes == 0) {
        return true; // Nothing to transmit
    }

    // Capture main task handle for ISR completion signaling
    mMainTaskHandle = xTaskGetCurrentTaskHandle();

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

    // Initialize counters
    mIsrContext->mIsrCount = 0;
    mIsrContext->mBytesTransmitted = 0;
    mIsrContext->mChunksCompleted = 0;
    mIsrContext->mTransmissionActive = true;
    mIsrContext->mEndTimeUs = 0;

    // Pre-populate ring buffers (fill all buffers if possible)
    while (hasRingSpace() && populateNextDMABuffer()) {
        // Buffer populated into ring
    }

    // Get actual number of buffers populated
    size_t buffers_populated = mIsrContext->mRingCount;

    // Verify at least one buffer was populated
    if (buffers_populated == 0) {
        FL_WARN("PARLIO: No buffers populated - cannot start transmission");
        mErrorOccurred = true;
        return false;
    }

    // Enable PARLIO TX unit for this transmission
    esp_err_t err = parlio_tx_unit_enable(mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit: " << err);
        mErrorOccurred = true;
        return false;
    }

    // Queue first buffer to start transmission
    FL_LOG_PARLIO("PARLIO: Starting ISR-based streaming | first_buffer_size="
           << mRingBufferSizes[0] << " | buffers_ready=" << buffers_populated);

    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000;

    size_t first_buffer_size = mRingBufferSizes[0];

    err = parlio_tx_unit_transmit(
        mTxUnit, mRingBufferPtrs[0],  // Use cached pointer for optimization
        first_buffer_size * 8, &tx_config);

    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to queue first buffer: " << err);
        mErrorOccurred = true;
        return false;
    }

    // Advance read index to consume the first buffer
    mIsrContext->mRingReadIdx = 1;
    mIsrContext->mRingCount = buffers_populated - 1;

    // Mark transmission started
    mIsrContext->mTransmitting = true;

    //=========================================================================
    // Create worker task for background DMA buffer population
    //=========================================================================
    // Using FreeRTOS high-priority task instead of true ISR for:
    // - Better portability (no undocumented ESP-IDF internals)
    // - Easier debugging (task can be inspected, not pure ISR)
    // - Similar performance (highest user priority preempts normal tasks)
    //
    // Task notification pattern:
    // - txDoneCallback calls vTaskNotifyGiveFromISR() when buffers need refilling
    // - Worker task waits on ulTaskNotifyTake() and populates buffers
    // - Task exits when mWorkerIsrEnabled becomes false
    //=========================================================================

    // Enable worker task
    mIsrContext->mWorkerIsrEnabled = true;

    // Create worker task with highest user priority
    BaseType_t taskResult = xTaskCreate(
        workerTaskFunction,          // Task function
        "parlio_worker",              // Task name (for debugging)
        4096,                         // Stack size in bytes
        this,                         // Task parameter (this pointer)
        configMAX_PRIORITIES - 1,     // Priority (highest user priority, below ISRs)
        reinterpret_cast<TaskHandle_t*>(&mWorkerTaskHandle)  // Task handle output
    );

    if (taskResult != pdPASS || !mWorkerTaskHandle) {
        FL_WARN("PARLIO: Failed to create worker task - falling back to blocking mode");
        mIsrContext->mWorkerIsrEnabled = false;
        mWorkerTaskHandle = nullptr;
        // Continue with transmission but without worker task assistance
    }

    FL_LOG_PARLIO("PARLIO: Worker task " << (mWorkerTaskHandle ? "created" : "disabled")
           << " | buffers_ready=" << buffers_populated);

    // Wait for transmission to complete (block on task notification from ISR)
    // ISR will signal this task when transmission is complete
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Disable PARLIO hardware after completion
    esp_err_t disable_err = parlio_tx_unit_disable(mTxUnit);
    if (disable_err != ESP_OK) {
        FL_WARN("PARLIO: Failed to disable TX unit after transmission: " << disable_err);
    }

    //=========================================================================
    // Cleanup worker task
    //=========================================================================
    if (mWorkerTaskHandle) {
        // Disarm worker task (signals task to exit loop)
        mIsrContext->mWorkerIsrEnabled = false;
        PARLIO_MEMORY_BARRIER();

        // Wake up task if it's blocked waiting for notification
        xTaskNotifyGive(static_cast<TaskHandle_t>(mWorkerTaskHandle));

        // Give task time to exit gracefully (10ms should be plenty)
        vTaskDelay(pdMS_TO_TICKS(10));

        // Task will delete itself via vTaskDelete(NULL) in workerTaskFunction
        mWorkerTaskHandle = nullptr;
    }

    return true; // Transmission completed successfully
}

ParlioEngineState ParlioEngine::poll() {
    if (!mInitialized || !mTxUnit || !mIsrContext) {
        return ParlioEngineState::READY;
    }

    // Check for errors
    if (mErrorOccurred) {
        FL_WARN("PARLIO: Error occurred during transmission");
        mIsrContext->mTransmitting = false;
        mErrorOccurred = false;
        return ParlioEngineState::ERROR;
    }

    // Check if streaming is complete
    if (mIsrContext->mStreamComplete) {
        // Execute memory barrier to synchronize all ISR writes
        PARLIO_MEMORY_BARRIER();

        // Clear completion flags
        mIsrContext->mTransmitting = false;
        mIsrContext->mStreamComplete = false;

        // Wait for final chunk to complete
        esp_err_t err = parlio_tx_unit_wait_all_done(mTxUnit, 0);

        if (err == ESP_OK) {
            // Disable PARLIO to reset peripheral state
            err = parlio_tx_unit_disable(mTxUnit);
            if (err != ESP_OK) {
                FL_WARN("PARLIO: Failed to disable TX unit: " << err);
            }

            // Short delay for GPIO stabilization
            fl::delayMicroseconds(100);

            return ParlioEngineState::READY;
        } else if (err == ESP_ERR_TIMEOUT) {
            return ParlioEngineState::BUSY;
        } else {
            FL_WARN("PARLIO: Error waiting for final chunk: " << err);
            return ParlioEngineState::ERROR;
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

    return ParlioEngineState::BUSY;
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
    PARLIO_MEMORY_BARRIER();

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

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
