/// @file channel_engine_parlio.h
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/C6/H2/C5
///
/// This file implements a ChannelEngine that uses ESP32's Parallel IO (PARLIO)
/// peripheral to drive multiple WS2812/WS2812B LED strips simultaneously on
/// parallel GPIO pins.
///
/// ## Hardware Requirements
/// - ESP32-P4, ESP32-C6, ESP32-H2, or ESP32-C5 (only variants with PARLIO
/// peripheral)
/// - 1-16 WS2812/WS2812B LED strips (power-of-2 widths: 1, 2, 4, 8, 16)
/// - Configurable GPIO pins (default: GPIO 1-16)
/// - **ESP32-C6**: Requires ESP-IDF 5.5.0 or later (earlier versions have a
/// known PARLIO driver bug)
///
/// ## Features
/// - **Multi-Channel Support**: Drive 1-16 LED strips simultaneously
/// - **Power-of-2 Optimization**: Native support for 1, 2, 4, 8, 16 channels
/// - **Non-Power-of-2 Support**: Automatic dummy lane management for 3, 5-7,
/// 9-15 channels
/// - **WS2812 Timing**: Accurate 8.0 MHz clock with 10-tick encoding (125ns per
/// tick)
/// - **Async Operation**: Non-blocking transmission with pollDerived() state
/// tracking
/// - **Large LED Support**: Automatic chunking with width-adaptive sizing
/// - **ISR Transposition**: Direct waveform encoding and bit transposition in
/// ISR callback
/// - **ISR Streaming**: Memory-safe ping-pong buffering with owned channel
/// pointer storage
///
/// ## Usage Example
/// ```cpp
/// // ChannelEnginePARLIO is automatically registered with ChannelBusManager
/// // when FASTLED_ESP32_HAS_PARLIO is enabled. Simply use FastLED's standard
/// API:
///
/// CRGB leds[100];
/// void setup() {
///     FastLED.addLeds<WS2812, 1, GRB>(leds, 100);  // GPIO 1
///     // PARLIO engine auto-selects 1/2/4/8/16-bit mode based on channel count
/// }
///
/// void loop() {
///     // Update LED data
///     fill_rainbow(leds, 100, 0, 7);
///     FastLED.show();  // Non-blocking transmission via PARLIO
/// }
/// ```
///
/// ## Performance Characteristics
/// - **Frame Rate**: 60+ FPS for typical LED counts (<500 LEDs per strip)
/// - **Chunk Size**: 100 LEDs per chunk (optimized for streaming)
/// - **Memory Usage** (for 1000 LEDs × 8 strips):
///   - Scratch buffer: 24 KB (per-strip RGB data)
///   - DMA ping-pong buffers: 48 KB (2 × 100 LEDs × 30 bytes per LED × 8 lanes)
///   - **Total: ~72 KB** (waveform expansion happens in ISR, not stored)
/// - **CPU Overhead**: Minimal - ISR performs waveform encoding and
/// transposition on-the-fly
///
/// ⚠️  **CRITICAL AI AGENT WARNING: NO LOGGING IN HOT PATHS**
///
/// The main thread computation loop (populateNextDMABuffer + blocking loop in
/// beginTransmission) is a CRITICAL PERFORMANCE HOT PATH. Adding ANY logging
/// (FL_LOG_PARLIO, FL_WARN, FL_DBG, printf) to these sections causes:
///
/// - 98× performance degradation (1.2s vs 12ms per transmission)
/// - Ring buffer underruns (hardware drains faster than CPU can refill)
/// - "Hardware was idle" errors (45+ per transmission)
///
/// **Root Cause**: UART at 115200 baud = ~9ms per 80-char log message
/// **CPU Budget**: Only 600μs available per buffer (hardware transmission time)
///
/// See TASK.md UPDATE #2 and #3 for detailed investigation and performance
/// analysis. DO NOT add logging to hot paths without explicit permission.
///
/// ## Technical Details
///
/// ### WS2812 Timing
/// PARLIO clock: 8.0 MHz (125ns per tick)
/// - Bit 0: 3 ticks high, 7 ticks low (375ns high, 875ns low)
/// - Bit 1: 7 ticks high, 3 ticks low (875ns high, 375ns low)
/// - Each LED byte → 80 bits (8 bits × 10 ticks)
/// - Each RGB LED → 240 bits total (24 bits × 10 ticks)
///
/// ### ISR Transposition Algorithm
/// PARLIO requires data in bit-parallel format (one byte per clock tick across
/// all strips). The implementation uses the generic waveform generator
/// (fl::channels::waveform_generator) with custom bit-packing for PARLIO's
/// parallel format:
///
/// **Input** (per-strip layout in scratch buffer):
///   ```
///   Strip 0: [R0, G0, B0, R1, G1, B1, ...]
///   Strip 1: [R0, G0, B0, R1, G1, B1, ...]
///   ...
///   Strip 7: [R0, G0, B0, R1, G1, B1, ...]
///   ```
///
/// **ISR Processing** (per LED byte):
/// 1. **Waveform Expansion**: Use fl::expandByteToWaveforms() with precomputed
/// bit0/bit1 patterns
///    - Each byte → 32 pulse bytes (0xFF=HIGH, 0x00=LOW)
/// 2. **Bit-Pack Transpose**: Convert byte-based waveforms to PARLIO's
/// bit-packed format
///    - Each pulse byte → 1 bit, packed across 8 lanes into output bytes
///
/// **Final Output** (bit-parallel DMA buffer):
///   ```
///   Byte 0:  [S7_p0, S6_p0, ..., S1_p0, S0_p0]  // Pulse 0 from all strips
///   Byte 1:  [S7_p1, S6_p1, ..., S1_p1, S0_p1]  // Pulse 1 from all strips
///   ...
///   Byte 31: [S7_p31, S6_p31, ..., S1_p31, S0_p31]  // Pulse 31 from all
///   strips (Pattern repeats for each LED color component)
///   ```
///
/// ### Buffer Size Calculation
/// Formula (from README.md):
/// ```
/// buffer_size = (num_leds × 240 bits × data_width + 7) / 8 bytes
/// ```
///
/// Examples (8-lane configuration):
/// - 100 LEDs × 8 lanes: (100 × 240 × 8 + 7) / 8 = 24,000 bytes
/// - 500 LEDs × 8 lanes: (500 × 240 × 8 + 7) / 8 = 120,000 bytes
/// - 1000 LEDs × 8 lanes: (1000 × 240 × 8 + 7) / 8 = 240,000 bytes
///
/// ### ISR-Based Streaming with Ping-Pong Buffering
/// To minimize memory usage while supporting arbitrary LED counts:
/// - **Direct ISR encoding**: Waveform expansion happens in ISR from per-strip
/// scratch buffer
/// - **Double buffering**: Two small DMA buffers (~100 LEDs each, 1-lane: ~3 KB
/// per buffer, 8-lane: ~24 KB per buffer)
/// - **ISR callback**: Hardware triggers callback when chunk transmission
/// completes
/// - **Automatic refilling**: While DMA transmits buffer A, ISR
/// encodes/transposes into buffer B
/// - **Chunk size**: 100 LEDs (fixed), balancing memory vs. ISR frequency
///
/// **Architecture benefits:**
/// - Minimal memory usage (no intermediate transposition buffers)
/// - Simple per-strip layout (no pre-processing required)
/// - Waveform expansion only when needed (no upfront 32x memory blowup)
///
/// ## Polymorphic Channel Count Support
/// The engine automatically selects optimal data width per batch:
///
/// | Channels | Selected Engine | Data Width | Dummy Lanes | Memory per 100
/// LEDs |
/// |----------|-----------------|------------|-------------|---------------------|
/// | 1        | 1-bit engine    | 1-bit      | 0           | ~1.2 KB | | 2 |
/// 2-bit engine    | 2-bit      | 0           | ~2.4 KB             | | 3-4 |
/// 4-bit engine    | 4-bit      | 1-0         | ~4.8 KB             | | 5-8 |
/// 8-bit engine    | 8-bit      | 3-0         | ~9.6 KB             | | 9-16 |
/// 16-bit engine   | 16-bit     | 7-0         | ~19.2 KB            |
///
/// - Engine selection happens per-batch in beginTransmission()
/// - Non-power-of-2 channel counts use dummy lanes (kept LOW, no LED artifacts)
/// - Maximum capability allocated at construction (all 1/2/4/8/16-bit engines)
///
/// ## Limitations
/// - **Platform-Specific**: Only available on ESP32-P4, ESP32-C6, ESP32-H2,
/// ESP32-C5 with PARLIO peripheral
/// - **ESP32-C6 Version Requirement**: ESP-IDF 5.5.0+ required (earlier
/// versions have PARLIO driver bug)
/// - **Channel Count**: 1-16 channels supported
/// - **Fixed Pins**: Uses default GPIO 1-16 (future: user-configurable)
///
/// ## See Also
/// - Implementation: `channel_engine_parlio.cpp`
/// - Unit Tests: `tests/fl/channels/parlio.cpp`
/// - Example Sketch: `examples/ParlioTest/ParlioTest.ino`
/// - Feature Flag: `FASTLED_ESP32_HAS_PARLIO` in
/// `src/platforms/esp/32/feature_flags/enabled.h`
/// - Reference: WLED PARLIO driver
/// (https://github.com/troyhacks/WLED/blob/P4_experimental/wled00%2Fparlio.cpp)

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/engine_events.h"
#include "fl/stl/array.h"
#include "fl/stl/deque.h"
#include "fl/stl/span.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/vector.h"
#include "parlio_engine.h"

// Forward declarations for PARLIO types (avoid including driver headers in .h)
struct parlio_tx_unit_t;
typedef struct parlio_tx_unit_t *parlio_tx_unit_handle_t;

// Forward declaration for heap_caps_free (defined in esp_heap_caps.h)
extern "C" void heap_caps_free(void *ptr);

//=============================================================================
// Hardware Capability Constants
//=============================================================================

// Maximum supported PARLIO data width per chip.
// Based on SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH from ESP-IDF soc_caps.h:
//
// | Chip     | Max Data Width |
// |----------|----------------|
// | ESP32-P4 | 16-bit         |
// | ESP32-C6 | 16-bit         |
// | ESP32-H2 | 8-bit          |
// | ESP32-C5 | 8-bit          |
//
// Official ESP-IDF PARLIO Documentation:
// - ESP32-P4:
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/parlio/index.html
// - ESP32-C6:
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/peripherals/parlio/index.html
// - ESP32-H2:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/parlio.html
// - ESP32-C5:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/peripherals/parlio/index.html
//
// Runtime validation against SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH occurs in
// the .cpp implementation file where platform-specific headers are included.
#define FASTLED_PARLIO_MAX_DATA_WIDTH 16

namespace fl {

//=============================================================================
// Helper Functions
//=============================================================================

/// @brief Select optimal PARLIO data width for given channel count
/// @param channel_count Number of actual channels (1-16)
/// @return Data width (1, 2, 4, 8, or 16), or 0 if invalid
/// @note This helper is used internally by ChannelEnginePARLIOImpl for
/// validation.
///       The polymorphic ChannelEnginePARLIO wrapper uses simplified 8/16-bit
///       selection.
/// @note Hardware capability validation happens at runtime in the .cpp file
///       using SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH.
inline size_t selectDataWidth(size_t channel_count) {
    // Validate input range (assume 16-bit max for all PARLIO chips)
    if (channel_count == 0 || channel_count > FASTLED_PARLIO_MAX_DATA_WIDTH)
        return 0;

    // Select smallest power-of-2 width that fits the channel count
    if (channel_count <= 1)
        return 1;
    if (channel_count <= 2)
        return 2;
    if (channel_count <= 4)
        return 4;
    if (channel_count <= 8)
        return 8;
    return 16;
}

/// @brief DEPRECATED - Calculate bytes per LED for given data width
/// @deprecated This function is NO LONGER USED. The calculation was moved to
/// runtime in initializeIfNeeded() (line 707-714) because:
/// 1. pulses_per_bit is determined at runtime (not compile-time constant)
/// 2. The formula was incorrect (assumed bit-packing but
/// transposeAndQueueNextChunk writes full bytes)
/// @param data_width PARLIO data width (1, 2, 4, 8, or 16)
/// @return Bytes per LED after waveform expansion
inline size_t calculateBytesPerLed(size_t data_width) {
    // WARNING: This formula is INCORRECT and caused 8x buffer overrun bug
    // (Iteration 2). Kept for reference only - DO NOT USE.
    // Bug: divided by 8 assuming bit-packing, but transposeAndQueueNextChunk()
    // writes 1 full byte per pulse tick (not bit-packed).
    if (data_width < 8) {
        return 96; // 768 bits / 8 bits per byte (WRONG - should be 768 bytes)
    } else {
        return 3 * 32 * (data_width / 8);
    }
}

/// @brief DEPRECATED - Calculate optimal chunk size for given data width
/// @deprecated This function is NO LONGER USED. The calculation was moved to
/// runtime in initializeIfNeeded() (line 716) to use actual pulses_per_bit.
/// @param data_width PARLIO data width (1, 2, 4, 8, or 16)
/// @return LEDs per chunk (optimized for ~2KB buffer size)
inline size_t calculateChunkSize(size_t data_width) {
    // WARNING: This function uses calculateBytesPerLed() which is INCORRECT.
    // Kept for reference only - DO NOT USE.
    constexpr size_t target_buffer_size = 2048;
    size_t bytes_per_led = calculateBytesPerLed(data_width);
    size_t chunk_size = target_buffer_size / bytes_per_led;

    // Clamp to reasonable range
    if (chunk_size < 10)
        chunk_size = 10;
    if (chunk_size > 500)
        chunk_size = 500;

    return chunk_size;
}

//=============================================================================
// Class Declaration
//=============================================================================

//=============================================================================
// Phase 4: ISR Context - Cache-Aligned Singleton Structure
//=============================================================================
// All ISR-related state consolidated into single 64-byte aligned struct for:
// - Improved cache line performance (single cache line access)
// - Prevention of false sharing between ISR and main thread
// - Clear separation between volatile (ISR-shared) and non-volatile fields
//
// Memory Synchronization Model:
// - ISR writes to volatile fields (stream_complete, transmitting, current_led)
// - Main thread reads volatile fields directly (compiler ensures fresh read)
// - After detecting stream_complete==true, main thread executes memory barrier
// - Memory barrier ensures all ISR writes visible before reading other fields
// - Non-volatile fields (isr_count, etc.) read after barrier are guaranteed consistent
struct alignas(64) ParlioIsrContext {
    // === Volatile Fields (shared between ISR and main thread) ===
    // These fields are written by ISR, read by main thread
    // Marked volatile to prevent compiler optimizations that would cache values

    volatile bool stream_complete;  ///< ISR sets true when transmission complete (ISR writes, main reads)
    volatile bool transmitting;     ///< ISR updates during transmission lifecycle (ISR writes, main reads)
    volatile size_t current_led;    ///< ISR updates LED position marker (ISR writes, main reads)

    // Phase 0: Ring buffer pointers for streaming DMA
    // ISR reads from read_ptr, CPU writes to write_ptr
    // Communication protocol: ISR increments read_ptr after consuming buffer (signals CPU)
    volatile size_t ring_read_ptr;  ///< ISR reads from this index (ISR writes, main reads)
    volatile size_t ring_write_ptr; ///< CPU writes to this index (main writes, ISR reads)
    volatile bool ring_error;       ///< Ring underflow/overflow error flag (ISR writes, main reads)

    // === Non-Volatile Fields (main thread synchronization required) ===
    // These fields are written by ISR but read by main thread ONLY after memory barrier
    // NOT marked volatile - main thread uses explicit barrier after reading stream_complete

    size_t total_leds;              ///< Total LEDs to transmit (main sets, ISR reads - no race)
    size_t num_lanes;               ///< Number of parallel lanes (main sets, ISR reads - Phase 5: moved from ParlioState)
    size_t ring_size;               ///< Number of buffers in ring (main sets, ISR reads - Phase 0)

    // Diagnostic counters (ISR writes, main reads after barrier)
    // Note: Using simple increment operations, synchronized via memory barrier
    uint32_t isr_count;             ///< ISR callback invocation count
    uint32_t bytes_transmitted;     ///< Total bytes transmitted this frame
    uint32_t chunks_completed;      ///< Number of chunks completed this frame

    // Phase 0: Buffer accounting for simplified ISR
    uint32_t buffers_submitted;         ///< Total buffers submitted to hardware (main thread writes)
    volatile uint32_t buffers_completed; ///< ISR increments, CPU polls (volatile required per LOOP.md line 34)
    uint32_t buffers_total;             ///< Total buffers for entire transmission (main thread sets)

    // ISR Debug Counters (Iteration 1: diagnose why ISR not submitting buffers)
    volatile uint32_t isr_submit_attempted; ///< ISR attempted to submit buffer
    volatile uint32_t isr_submit_success;   ///< ISR successfully submitted buffer
    volatile uint32_t isr_submit_failed;    ///< ISR failed to submit buffer
    volatile uint32_t isr_ring_empty;       ///< ISR found ring empty (read_ptr == write_ptr)
    volatile uint32_t isr_all_buffers_done; ///< ISR detected all buffers completed

    // Diagnostic fields (written by ISR, read by main thread after barrier)
    bool transmission_active;       ///< Debug: Transmission currently active
    uint64_t end_time_us;           ///< Debug: Transmission end timestamp (microseconds)

    // Debug: DMA buffer output tracking (main thread writes, Validation.ino reads)
    fl::deque<uint8_t> mDebugDmaOutput; ///< Copy of all DMA buffer data for validation (uses deque to avoid large contiguous allocation)

    // Constructor: Initialize all fields to safe defaults
    ParlioIsrContext()
        : stream_complete(false)
        , transmitting(false)
        , current_led(0)
        , ring_read_ptr(0)
        , ring_write_ptr(0)
        , ring_error(false)
        , total_leds(0)
        , num_lanes(0)
        , ring_size(0)
        , isr_count(0)
        , bytes_transmitted(0)
        , chunks_completed(0)
        , buffers_submitted(0)
        , buffers_completed(0)
        , buffers_total(0)
        , isr_submit_attempted(0)
        , isr_submit_success(0)
        , isr_submit_failed(0)
        , isr_ring_empty(0)
        , isr_all_buffers_done(0)
        , transmission_active(false)
        , end_time_us(0)
    {}

    // Singleton accessor for debug metrics
    static ParlioIsrContext* getInstance() {
        return s_instance;
    }

    // Singleton setter (called by ChannelEnginePARLIOImpl)
    static void setInstance(ParlioIsrContext* instance) {
        s_instance = instance;
    }

private:
    static ParlioIsrContext* s_instance;
};

// Phase 0: Ring buffer configuration for streaming DMA
// Testing with fewer, larger buffers to see if buffer size affects transmission
//
// CRITICAL: Buffer size formula (from README.md):
//   buffer_size = (num_leds × 240 bits × data_width + 7) / 8 bytes
//
// UNIVERSAL CONSTANT: 30 bytes per LED (RGB) regardless of data_width
//   - data_width multiplier represents LEDs transmitted in parallel, NOT overhead
//   - 1-lane: 30 bytes → 1 LED
//   - 8-lane: 240 bytes → 8 LEDs (30 bytes each)
//   - 16-lane: 480 bytes → 16 LEDs (30 bytes each)
//
// Testing configuration: 8 larger buffers
//   Each buffer: ~125 LEDs × 30 bytes/LED = ~3750 bytes (~3.7 KB)
//   Total ring: 8 × 3750 bytes = ~30 KB
constexpr size_t PARLIO_RING_BUFFER_COUNT = 8;


/// @brief Internal PARLIO implementation with fixed data width
///
/// This is the actual hardware driver implementation. It is used internally
/// by the polymorphic ChannelEnginePARLIO wrapper class.
///
/// @note This class should not be used directly - use ChannelEnginePARLIO
/// instead.
class ChannelEnginePARLIOImpl : public IChannelEngine {

  public:
    /// @brief Constructor with runtime data width selection
    /// @param data_width PARLIO data width (1, 2, 4, 8, or 16)
    explicit ChannelEnginePARLIOImpl(size_t data_width);
    ~ChannelEnginePARLIOImpl() override;

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    void show() override;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    EngineState poll() override;

    /// @brief Get the engine name for affinity binding
    /// @return "PARLIO"
    const char* getName() const override { return "PARLIO"; }

    void setReversedPinOrder(bool reversed_pin_order);

  private:
    /// @brief Begin LED data transmission for all enqueued channels
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief Prepare scratch buffer with per-lane data layout
    /// @param channelData Span of channel data to copy
    /// @param maxChannelSize Maximum channel size in bytes
    void prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                             size_t maxChannelSize);

  private:
    /// @brief Group of channels sharing the same chipset timing
    struct ChipsetGroup {
        ChipsetTimingConfig mTiming;              ///< Shared timing configuration
        fl::vector<ChannelDataPtr> mChannels;     ///< Channels in this group
    };

    /// @brief HAL engine reference (singleton)
    detail::ParlioEngine& mEngine;

    /// @brief Initialization flag
    bool mInitialized;

    /// @brief PARLIO data width (1, 2, 4, 8, or 16)
    size_t mDataWidth;

    /// @brief Scratch buffer for per-lane data layout (owned by channel engine)
    fl::vector<uint8_t> mScratchBuffer;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr>
        mEnqueuedChannels; ///< Channels enqueued via enqueue(), waiting for
                           ///< show()
    fl::vector<ChannelDataPtr>
        mTransmittingChannels; ///< Channels currently transmitting (for
                               ///< cleanup)

    /// @brief Chipset grouping state for sequential transmission
    fl::vector<ChipsetGroup> mChipsetGroups;      ///< Groups of channels by timing
    size_t mCurrentGroupIndex;                     ///< Index of currently transmitting group
    bool mReversedPinOrder;
};

//=============================================================================
// Polymorphic Wrapper Class
//=============================================================================

/// @brief PARLIO engine with lazy initialization and dynamic reconfiguration
///
/// This class manages a single PARLIO TX unit instance, creating and
/// reconfiguring it as needed based on the channel count. This architecture
/// matches the ESP32 hardware limitation: only ONE PARLIO TX unit exists per
/// chip.
///
/// ## Hardware Constraints (Per ESP32 Chip)
/// - **ESP32-C6/P4**: 1 TX unit, 16-bit max width
/// - **ESP32-H2/C5**: 1 TX unit, 8-bit max width
/// - **Critical**: PARLIO TX unit is a SINGLE shared resource - cannot be split
///   into multiple independent units
/// - **Data width cannot be changed** after initialization - must delete and
///   recreate
///
/// ## Architecture: Lazy Initialization
/// - Contains ONE ChannelEnginePARLIOImpl instance (created on-demand)
/// - On first show(), determines optimal width from channel count
/// - Creates TX unit with power-of-2 width: 1, 2, 4, 8, or 16-bit
/// - If channel count changes significantly, deletes and recreates with new
///   width
/// - Inherits from IChannelEngine for standard FastLED integration
/// - Managed by ChannelBusManager which handles frame lifecycle events
///
/// ## Lifecycle
/// 1. **Construction**: No hardware initialization (lazy)
/// 2. **First show()**:
///    - Determines optimal width from channel count
///    - Creates single ChannelEnginePARLIOImpl instance
///    - Initializes PARLIO TX unit with optimal width
/// 3. **Subsequent Frames**:
///    - If channel count fits current width: reuse engine
///    - If width needs to change: delete engine, create new one
/// 4. **Polling**: poll() checks engine state (if initialized)
/// 5. **Destruction**: Cleans up engine instance
///
/// ## Memory Management (Single Engine)
/// - Only ONE engine instance allocated at a time
/// - Memory usage depends on data width:
///   - 1-bit: ~1.2 KB (1 channel)
///   - 2-bit: ~2.4 KB (2 channels)
///   - 4-bit: ~4.8 KB (3-4 channels)
///   - 8-bit: ~9.6 KB (5-8 channels)
///   - 16-bit: ~19.2 KB (9-16 channels)
/// - Reconfiguration overhead: ~1-2ms (delete + create TX unit)
///
/// @note Only available on ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 with
/// PARLIO peripheral.
///       Compilation is guarded by FASTLED_ESP32_HAS_PARLIO feature flag.
class ChannelEnginePARLIO : public IChannelEngine {
  public:
    /// @brief Constructor - lazy initialization (no engine created)
    ChannelEnginePARLIO();
    ~ChannelEnginePARLIO() override;

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    void show() override;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    EngineState poll() override;

    /// @brief Get the engine name for affinity binding
    /// @return "PARLIO"
    const char* getName() const override { return "PARLIO"; }

  private:
    /// @brief Begin LED data transmission with lazy init and reconfiguration
    /// @param channelData Span of channel data to transmit
    /// @note Creates engine on first use, reconfigures if width changes
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

  private:
    /// @brief Single engine instance (created on-demand, reconfigured as
    /// needed)
    /// @note Using fl::unique_ptr for RAII - automatic cleanup, no manual
    /// delete
    fl::unique_ptr<ChannelEnginePARLIOImpl> mEngine;

    /// @brief Current data width (0 = not initialized)
    size_t mCurrentDataWidth;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr>
        mEnqueuedChannels; ///< Channels enqueued via enqueue(), waiting for
                           ///< show()
    fl::vector<ChannelDataPtr>
        mTransmittingChannels; ///< Channels currently transmitting (for
                               ///< cleanup)
};

//=============================================================================
// Debug Instrumentation (Phase 0)
//=============================================================================

/// @brief Debug metrics structure for PARLIO transmission analysis
struct ParlioDebugMetrics {
    uint64_t
        mStartTimeUs; ///< Timestamp when transmission begins (microseconds)
    uint64_t
        mEndTimeUs; ///< Timestamp when transmission completes (microseconds)
    uint32_t mIsrCount;        ///< Number of ISR callbacks fired
    uint32_t mChunksQueued;    ///< Number of chunks queued for transmission
    uint32_t mChunksCompleted; ///< Number of chunks that completed transmission
    uint32_t mBytesTotal;      ///< Total bytes expected to transmit
    uint32_t mBytesTransmitted; ///< Total bytes actually transmitted
    uint32_t mErrorCode;        ///< ESP-IDF error code (0 = success)
    bool mTransmissionActive;   ///< True if transmission is in progress
};

/// @brief Get current PARLIO debug metrics
/// @return Debug metrics structure with transmission statistics
/// @note This function is safe to call from any context
ParlioDebugMetrics getParlioDebugMetrics();

//=============================================================================
// Factory Function
//=============================================================================

/// @brief Create PARLIO engine instance with lazy initialization
/// @return Shared pointer to IChannelEngine, or nullptr if creation fails
/// @note Engine uses lazy initialization - hardware is configured on first
/// show()
/// @note Auto-selects optimal data width based on channel count (1, 2, 4, 8, or
/// 16-bit)
/// @note Reconfigures width dynamically if channel count changes significantly
fl::shared_ptr<IChannelEngine> createParlioEngine();

} // namespace fl

#endif // ESP32
