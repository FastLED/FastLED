/// @file parlio_engine.h
/// @brief Low-level PARLIO hardware abstraction layer (HAL) for ESP32
///
/// ⚠️ ⚠️ ⚠️  CRITICAL AI AGENT WARNING: NO LOGGING IN HOT PATHS ⚠️ ⚠️ ⚠️
///
/// This HAL contains CRITICAL PERFORMANCE HOT PATHS that are called 20+ times
/// per transmission in tight timing loops competing with hardware:
///
/// **HOT PATH FUNCTIONS (NO LOGGING ALLOWED):**
/// - `populateNextDMABuffer()` - Main thread computation loop
/// - `txDoneCallback()` - ISR callback (runs in interrupt context)
/// - `populateDmaBuffer()` - DMA buffer generation helper
/// - The blocking loop inside `beginTransmission()` after first buffer queued
///
/// **WHY LOGGING IS FORBIDDEN:**
/// - UART overhead: ~9ms per log call @ 115200 baud (80 chars/log)
/// - CPU budget: Only 600μs available per buffer (hardware transmission time)
/// - Performance impact: Logging causes 98× slowdown (1.2s vs 12ms per frame)
/// - Ring buffer underruns: Hardware drains faster than CPU can refill
/// - System crashes: ISR logging can cause watchdog timeouts
///
/// **ALLOWED LOGGING:**
/// - ✅ One-time initialization logs (before hot path entry)
/// - ✅ Error conditions (infrequent, non-hot path)
/// - ✅ Summary logs AFTER transmission completes
/// - ❌ FORBIDDEN: FL_LOG_PARLIO, FL_WARN, FL_DBG, printf in hot paths
///
/// **IF YOU NEED TO DEBUG HOT PATHS:**
/// 1. Use logic analyzer or oscilloscope (hardware timing)
/// 2. Increment counters and log AFTER transmission completes
/// 3. Enable logging ONLY for single-shot debugging, then REMOVE IT
///
/// See channel_engine_parlio.cpp lines 60-74, 399-426, 686-707, 1206-1226
/// and TASK.md UPDATE #2/#3 for detailed investigation of logging impact.
///
/// ⚠️ ⚠️ ⚠️  DO NOT ADD LOGGING TO HOT PATHS - YOU WILL BREAK THE CODE ⚠️ ⚠️ ⚠️
///
/// **KNOWN LIMITATIONS:**
///
/// 1. **Buffer-Boundary Corruption (ESP32-C6 PARLIO Hardware Bug)**
///    - Accuracy: 99.97% (1 LED in 3000 may show corruption)
///    - Location: Last LED in first DMA buffer (typically LED ~103 for large strips)
///    - Signature: +1 bit on blue channel (0xAA → 0xAB), timing glitch (H 125ns vs H 500ns)
///    - Root cause: PARLIO peripheral hardware-level timing bug during buffer transition
///    - Investigation: 6 iterations, 9 mitigation strategies tested, ZERO successes
///      * Cache coherency (non-cacheable memory) - FAILED
///      * Memory ordering (RISC-V fence) - FAILED
///      * Cache line alignment - FAILED
///      * Buffer transition delays (50µs) - FAILED
///      * Padding workarounds - FAILED (caused system hangs)
///    - Recommendation: Accept 99.97% accuracy as documented limitation
///    - Alternatives: Switch to RMT driver (100% accuracy) or report bug to Espressif
///    - See PARLIO_RESEARCH.md for complete investigation details
///
/// 2. **ESP-IDF Version Dependency**
///    - Requires Arduino-ESP32 v5.5.0+ for cache sync APIs
///    - esp_cache_msync() returns ESP_ERR_INVALID_ARG (258) for DMA memory (expected)
///    - Error is benign - function provides beneficial side-effects despite error code

#pragma once

#include "fl/compiler_control.h"
#include "fl/isr.h"
#include "fl/singleton.h"

#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"

// Include extracted headers
#include "parlio_isr_context.h"
#include "parlio_buffer_calc.h"
#include "parlio_debug.h"
#include "parlio_ring_buffer.h"

// Include peripheral interface
#include "iparlio_peripheral.h"


namespace fl {
namespace detail {

//=============================================================================
// Engine State Enum
//=============================================================================

enum class ParlioEngineState {
    READY,      ///< Engine ready for new transmission (idle)
    BUSY,       ///< Engine actively blocked (never happens in current async implementation)
    DRAINING,   ///< Transmission in progress (ISR-driven, async)
    ERROR       ///< Error occurred during transmission
};

//=============================================================================
// PARLIO Engine - Hardware Abstraction Layer
//=============================================================================

/// @brief Singleton PARLIO hardware engine for ESP32
///
/// This class encapsulates all PARLIO hardware management, ISR handling,
/// and DMA transmission logic. The channel engine uses this HAL to submit
/// LED data without dealing with hardware details.
///
/// ## Lifecycle
/// 1. Get instance via getInstance()
/// 2. Call initialize() with hardware config (one-time setup)
/// 3. Call beginTransmission() to start LED data transmission (blocking)
/// 4. Call poll() to check status and continue populating buffers
/// 5. Repeat steps 3-4 for each frame
///
/// ## Memory Layout
/// Caller provides scratch buffer with per-lane layout:
/// ```
/// [lane0_data (laneStride bytes)][lane1_data (laneStride bytes)]...[laneN_data]
/// ```
/// Engine generates DMA buffers with PARLIO bit-parallel layout.
///
/// ## Ring Buffer Memory Management
/// - Total buffer size capped at FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES
/// - Default caps: 256 KB (ESP32-C6/S3), 512 KB (ESP32-P4)
/// - Override via build flags: -DFASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES=<bytes>
/// - When cap exceeded, system uses streaming mode (multiple iterations)
/// - Performance impact: ~30 µs overhead per streaming iteration
/// - Prevents OOM on constrained platforms while maintaining functionality
///
/// ## Interrupt Priority Configuration
/// - Default ISR priority: Level 3 (highest for C handlers on ESP32)
/// - Override via build flag: -DFL_ESP_PARLIO_ISR_PRIORITY=<level>
/// - Level 3 is recommended maximum (Level 4+ requires assembly handlers)
/// - WiFi runs at level 4, so PARLIO ISRs may be preempted during WiFi activity
/// - Supported range: 1 (low) to 3 (high) for C handlers
///
/// ## Thread Safety
/// - initialize() is NOT thread-safe (call once during setup)
/// - beginTransmission() blocks until complete (no concurrent calls)
/// - poll() safe to call anytime
/// - ISR runs asynchronously, uses memory barriers for synchronization
class ParlioEngine {
public:
    /// @brief Get singleton instance
    static ParlioEngine& getInstance();

    /// @brief Initialize PARLIO hardware (one-time setup)
    /// @param dataWidth PARLIO data width (1, 2, 4, 8, or 16)
    /// @param pins GPIO pins for each lane (size must equal dataWidth)
    /// @param timing Chipset timing configuration (T1, T2, T3)
    /// @param maxLedsPerChannel Maximum LEDs per channel (for buffer sizing)
    /// @return true on success, false on error
    bool initialize(size_t dataWidth,
                   const fl::vector<int>& pins,
                   const ChipsetTimingConfig& timing,
                   size_t maxLedsPerChannel);

    /// @brief Begin LED data transmission (blocking until complete)
    /// @param scratchBuffer Per-lane scratch buffer (caller-owned)
    /// @param totalBytes Total bytes in scratch buffer (all lanes)
    /// @param numLanes Number of active lanes (1 to dataWidth)
    /// @param laneStride Bytes per lane in scratch buffer
    /// @return true on success, false on error
    ///
    /// ⚠️  CRITICAL: This function contains HOT PATH code (blocking loop)
    /// DO NOT add logging inside the blocking loop after first buffer submission
    bool beginTransmission(const uint8_t* scratchBuffer,
                          size_t totalBytes,
                          size_t numLanes,
                          size_t laneStride);

    /// @brief Poll engine state and continue buffer population
    /// @return Current engine state (READY, BUSY, or ERROR)
    ParlioEngineState poll();

    /// @brief Check if transmission is in progress
    bool isTransmitting() const;

    /// @brief Get debug metrics for transmission analysis
    ParlioDebugMetrics getDebugMetrics() const;

    /// @brief Destructor - cleans up PARLIO hardware
    ~ParlioEngine();

private:
    // Singleton pattern - allow Singleton<T> to construct instance
    friend class fl::Singleton<ParlioEngine>;

    ParlioEngine();
    ParlioEngine(const ParlioEngine&) = delete;
    ParlioEngine& operator=(const ParlioEngine&) = delete;

    /// @brief ISR callback for transmission completion
    /// ⚠️  CRITICAL: NO LOGGING IN THIS FUNCTION - Runs in interrupt context
    /// ⚠️  See ISR SAFETY RULES in ParlioIsrContext documentation
    /// @param tx_unit Opaque handle to TX unit (void* for interface compatibility)
    /// @param edata Event data (unused)
    /// @param user_ctx User context (ParlioEngine* instance)
    static bool txDoneCallback(void* tx_unit,
                              const void* edata,
                              void* user_ctx);

    /// @brief Worker function for DMA buffer population (called from txDoneCallback)
    /// ⚠️  CRITICAL ISR SAFETY RULES:
    /// ⚠️  1. NO LOGGING (FL_LOG_PARLIO, FL_WARN, FL_DBG, printf, etc.)
    /// ⚠️  2. NO BLOCKING operations (mutex, delay, heap alloc)
    /// ⚠️  3. MINIMIZE execution time (<10µs ideal)
    /// ⚠️  4. ONLY ISR-safe operations
    /// @param user_data ParlioEngine instance pointer
    static void FL_IRAM workerIsrCallback(void* user_data);

    /// @brief Debug task function for periodic ISR state logging
    /// @param arg ParlioEngine instance pointer
    /// @note Runs at low priority (1), logs ISR state every 500ms during transmission
    static void debugTaskFunction(void* arg);

    /// @brief Populate a DMA buffer with waveform data
    /// ⚠️  CRITICAL HOT PATH - NO LOGGING IN IMPLEMENTATION
    bool populateDmaBuffer(uint8_t* outputBuffer,
                          size_t outputBufferCapacity,
                          size_t startByte,
                          size_t byteCount,
                          size_t& outputBytesWritten);

    /// @brief Populate next available DMA buffer (incremental)
    /// ⚠️  CRITICAL HOT PATH - NO LOGGING IN IMPLEMENTATION
    /// ⚠️  Called 20+ times per transmission in tight timing loop
    /// ⚠️  Logging causes 98× performance degradation
    bool populateNextDMABuffer();

    /// @brief Check if ring has space for more buffers
    bool hasRingSpace() const;

    /// @brief Allocate and initialize all ring buffers (one-time)
    bool allocateRingBuffers();

private:
    // Initialization state
    bool mInitialized;
    size_t mDataWidth;
    size_t mActualChannels;
    size_t mDummyLanes;

    // PARLIO peripheral interface (singleton - real hardware or mock)
    IParlioPeripheral* mPeripheral;

    // GPIO pin assignments
    fl::vector<int> mPins;

    // Timing configuration
    uint32_t mTimingT1Ns;
    uint32_t mTimingT2Ns;
    uint32_t mTimingT3Ns;
    uint32_t mResetUs;  // Reset time in microseconds

    // Wave8 lookup table
    fl::Wave8BitExpansionLut mWave8Lut;

    // ISR context (cache-aligned, 64 bytes)
    fl::unique_ptr<ParlioIsrContext> mIsrContext;

    // Main task handle for transmission completion signaling (TaskHandle_t)
    void* mMainTaskHandle;

    // Debug task for periodic ISR state logging (unified task API)
    fl::task mDebugTask;

    // Ring buffer for DMA streaming (fixed 3-buffer architecture)
    fl::unique_ptr<ParlioRingBuffer3> mRingBuffer;
    size_t mRingBufferCapacity;  // Capacity of each ring buffer (64-byte aligned)

    // Scratch buffer pointer (owned by caller, NOT by this class)
    const uint8_t* mScratchBuffer;
    size_t mLaneStride;

    // Error state
    bool mErrorOccurred;

    // TX unit enable state tracking (prevents double-disable errors)
    bool mTxUnitEnabled;
};

} // namespace detail
} // namespace fl
