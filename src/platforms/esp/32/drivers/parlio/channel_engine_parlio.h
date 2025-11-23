/// @file channel_engine_parlio.h
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/C6/H2/C5
///
/// This file implements a ChannelEngine that uses ESP32's Parallel IO (PARLIO) peripheral
/// to drive multiple WS2812/WS2812B LED strips simultaneously on parallel GPIO pins.
///
/// ## Hardware Requirements
/// - ESP32-P4, ESP32-C6, ESP32-H2, or ESP32-C5 (only variants with PARLIO peripheral)
/// - 1-16 WS2812/WS2812B LED strips (power-of-2 widths: 1, 2, 4, 8, 16)
/// - Configurable GPIO pins (default: GPIO 1-16)
///
/// ## Features
/// - **Multi-Channel Support**: Drive 1-16 LED strips simultaneously
/// - **Power-of-2 Optimization**: Native support for 1, 2, 4, 8, 16 channels
/// - **Non-Power-of-2 Support**: Automatic dummy lane management for 3, 5-7, 9-15 channels
/// - **WS2812 Timing**: Accurate 3.2 MHz clock with 4-tick encoding (312.5ns per tick)
/// - **Async Operation**: Non-blocking transmission with pollDerived() state tracking
/// - **Large LED Support**: Automatic chunking with width-adaptive sizing
/// - **ISR Transposition**: Direct waveform encoding and bit transposition in ISR callback
/// - **ISR Streaming**: Memory-safe ping-pong buffering with owned channel pointer storage
///
/// ## Usage Example
/// ```cpp
/// // ChannelEnginePARLIO is automatically registered with ChannelBusManager
/// // when FASTLED_ESP32_HAS_PARLIO is enabled. Simply use FastLED's standard API:
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
///   - DMA ping-pong buffers: 19.2 KB (2 × 100 LEDs × 96 bytes expanded waveforms)
///   - **Total: ~43 KB** (waveform expansion happens in ISR, not stored)
/// - **CPU Overhead**: Minimal - ISR performs waveform encoding and transposition on-the-fly
///
/// ## Technical Details
///
/// ### WS2812 Timing
/// PARLIO clock: 3.2 MHz (312.5ns per tick)
/// - Bit 0: 0b1000 (1 tick high, 3 ticks low = 312.5ns high, 937.5ns low)
/// - Bit 1: 0b1110 (3 ticks high, 1 tick low = 937.5ns high, 312.5ns low)
/// - Each LED byte → 32 bits (8 bits × 4 ticks)
/// - Each RGB LED → 96 bits total (24 bytes × 4 ticks)
///
/// ### ISR Transposition Algorithm
/// PARLIO requires data in bit-parallel format (one byte per clock tick across all strips).
/// The implementation uses the generic waveform generator (fl::channels::waveform_generator)
/// with custom bit-packing for PARLIO's parallel format:
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
/// 1. **Waveform Expansion**: Use fl::expandByteToWaveforms() with precomputed bit0/bit1 patterns
///    - Each byte → 32 pulse bytes (0xFF=HIGH, 0x00=LOW)
/// 2. **Bit-Pack Transpose**: Convert byte-based waveforms to PARLIO's bit-packed format
///    - Each pulse byte → 1 bit, packed across 8 lanes into output bytes
///
/// **Final Output** (bit-parallel DMA buffer):
///   ```
///   Byte 0:  [S7_p0, S6_p0, ..., S1_p0, S0_p0]  // Pulse 0 from all strips
///   Byte 1:  [S7_p1, S6_p1, ..., S1_p1, S0_p1]  // Pulse 1 from all strips
///   ...
///   Byte 31: [S7_p31, S6_p31, ..., S1_p31, S0_p31]  // Pulse 31 from all strips
///   (Pattern repeats for each LED color component)
///   ```
///
/// ### Buffer Size Calculation
/// For 8 lanes (fixed configuration):
/// ```
/// Total bytes = numLeds × 3 colors × 32 ticks × 1 byte per tick
/// ```
///
/// Examples:
/// - 100 LEDs × 8 strips: 100 × 3 × 32 × 1 = 9,600 bytes
/// - 500 LEDs × 8 strips: 500 × 3 × 32 × 1 = 48,000 bytes
/// - 1000 LEDs × 8 strips: 1000 × 3 × 32 × 1 = 96,000 bytes
///
/// ### ISR-Based Streaming with Ping-Pong Buffering
/// To minimize memory usage while supporting arbitrary LED counts:
/// - **Direct ISR encoding**: Waveform expansion happens in ISR from per-strip scratch buffer
/// - **Double buffering**: Two small DMA buffers (~100 LEDs each, ~9.6 KB per buffer)
/// - **ISR callback**: Hardware triggers callback when chunk transmission completes
/// - **Automatic refilling**: While DMA transmits buffer A, ISR encodes/transposes into buffer B
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
/// | Channels | Selected Engine | Data Width | Dummy Lanes | Memory per 100 LEDs |
/// |----------|-----------------|------------|-------------|---------------------|
/// | 1        | 1-bit engine    | 1-bit      | 0           | ~1.2 KB             |
/// | 2        | 2-bit engine    | 2-bit      | 0           | ~2.4 KB             |
/// | 3-4      | 4-bit engine    | 4-bit      | 1-0         | ~4.8 KB             |
/// | 5-8      | 8-bit engine    | 8-bit      | 3-0         | ~9.6 KB             |
/// | 9-16     | 16-bit engine   | 16-bit     | 7-0         | ~19.2 KB            |
///
/// - Engine selection happens per-batch in beginTransmission()
/// - Non-power-of-2 channel counts use dummy lanes (kept LOW, no LED artifacts)
/// - Maximum capability allocated at construction (all 1/2/4/8/16-bit engines)
///
/// ## Limitations
/// - **Platform-Specific**: Only available on ESP32-P4, ESP32-C6, ESP32-H2, ESP32-C5 with PARLIO peripheral
/// - **Channel Count**: 1-16 channels supported
/// - **Fixed Pins**: Uses default GPIO 1-16 (future: user-configurable)
///
/// ## See Also
/// - Implementation: `channel_engine_parlio.cpp`
/// - Unit Tests: `tests/fl/channels/parlio.cpp`
/// - Example Sketch: `examples/ParlioTest/ParlioTest.ino`
/// - Feature Flag: `FASTLED_ESP32_HAS_PARLIO` in `src/platforms/esp/32/feature_flags/enabled.h`
/// - Reference: WLED PARLIO driver (https://github.com/troyhacks/WLED/blob/P4_experimental/wled00%2Fparlio.cpp)

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/engine_events.h"
#include "fl/channels/waveform_generator.h"
#include "ftl/span.h"
#include "ftl/vector.h"
#include "ftl/array.h"

// Forward declarations for PARLIO types (avoid including driver headers in .h)
struct parlio_tx_unit_t;
typedef struct parlio_tx_unit_t* parlio_tx_unit_handle_t;

namespace fl {

//=============================================================================
// Helper Functions
//=============================================================================

/// @brief Select optimal PARLIO data width for given channel count
/// @param channel_count Number of actual channels (1-16)
/// @return Data width (1, 2, 4, 8, or 16), or 0 if invalid
/// @note This helper is used internally by ChannelEnginePARLIOImpl for validation.
///       The polymorphic ChannelEnginePARLIO wrapper uses simplified 8/16-bit selection.
inline size_t selectDataWidth(size_t channel_count) {
    if (channel_count == 0 || channel_count > 16) return 0;
    if (channel_count <= 1) return 1;
    if (channel_count <= 2) return 2;
    if (channel_count <= 4) return 4;
    if (channel_count <= 8) return 8;
    return 16;
}

/// @brief Calculate bytes per LED for given data width
/// @param data_width PARLIO data width (1, 2, 4, 8, or 16)
/// @return Bytes per LED after waveform expansion
inline size_t calculateBytesPerLed(size_t data_width) {
    // Formula: 3 colors × 32 ticks × (data_width / 8)
    return 3 * 32 * (data_width / 8);
}

/// @brief Calculate optimal chunk size for given data width
/// @param data_width PARLIO data width (1, 2, 4, 8, or 16)
/// @return LEDs per chunk (optimized for ~10KB buffer size)
inline size_t calculateChunkSize(size_t data_width) {
    // Target: ~10 KB per buffer for all widths
    constexpr size_t target_buffer_size = 10240;
    size_t bytes_per_led = calculateBytesPerLed(data_width);
    size_t chunk_size = target_buffer_size / bytes_per_led;

    // Clamp to reasonable range
    if (chunk_size < 10) chunk_size = 10;
    if (chunk_size > 500) chunk_size = 500;

    return chunk_size;
}

//=============================================================================
// Class Declaration
//=============================================================================

/// @brief Internal PARLIO implementation with fixed data width
///
/// This is the actual hardware driver implementation. It is used internally
/// by the polymorphic ChannelEnginePARLIO wrapper class.
///
/// @note This class should not be used directly - use ChannelEnginePARLIO instead.
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

private:
    /// @brief Begin LED data transmission for all enqueued channels
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

private:
    /// @brief PARLIO hardware state with ISR-based streaming support
    struct ParlioState {
        parlio_tx_unit_handle_t tx_unit;     ///< PARLIO TX unit handle
        fl::vector<int> pins;                 ///< GPIO pin assignments (gpio_num_t cast to int) - data_width pins
        volatile bool transmitting;           ///< Transmission in progress flag

        // Configuration (set during initialization)
        size_t data_width;                    ///< PARLIO data width: 1, 2, 4, 8, or 16 (runtime parameter)
        size_t actual_channels;               ///< User-requested channels: 1 to data_width
        size_t dummy_lanes;                   ///< Calculated: data_width - actual_channels

        // Double-buffered DMA streaming (DMA-capable memory for waveform output)
        uint8_t* buffer_a;                    ///< First DMA buffer (ping-pong, heap_caps_malloc)
        uint8_t* buffer_b;                    ///< Second DMA buffer (ping-pong, heap_caps_malloc)
        size_t buffer_size;                   ///< Size of each DMA buffer in bytes
        volatile uint8_t* active_buffer;      ///< Currently transmitting buffer
        volatile uint8_t* fill_buffer;        ///< Buffer being filled

        // Streaming state
        volatile size_t num_lanes;            ///< Number of parallel lanes (same as data_width)
        volatile size_t lane_stride;          ///< Bytes per lane (stride - all lanes same size)
        volatile size_t current_led;          ///< Current LED position in source data
        volatile size_t total_leds;           ///< Total LEDs to transmit
        volatile size_t leds_per_chunk;       ///< LEDs per DMA chunk

        // Synchronization
        volatile bool stream_complete;        ///< All chunks transmitted flag
        volatile bool error_occurred;         ///< Error flag for ISR

        // Scratch buffer (reusable, grows as needed)
        // Single segmented array: [lane0_data][lane1_data]...[laneN_data]
        // Each lane is lane_stride bytes (padded to max channel size)
        // Only allocated for actual channels, NOT dummy lanes (memory optimization)
        fl::vector<uint8_t> scratch_padded_buffer; ///< Contiguous buffer for actual channels only (regular heap, non-DMA)

        // Precomputed waveforms (generated from timing parameters during initialization)
        fl::array<uint8_t, 16> bit0_waveform;      ///< Waveform pattern for bit 0 (up to 16 pulses)
        fl::array<uint8_t, 16> bit1_waveform;      ///< Waveform pattern for bit 1 (up to 16 pulses)
        size_t pulses_per_bit;                     ///< Number of pulses per bit (varies by clock frequency)

        ParlioState(size_t width)
            : tx_unit(nullptr)
            , transmitting(false)
            , data_width(width)
            , actual_channels(0)
            , dummy_lanes(0)
            , buffer_a(nullptr)
            , buffer_b(nullptr)
            , buffer_size(0)
            , active_buffer(nullptr)
            , fill_buffer(nullptr)
            , num_lanes(width)
            , lane_stride(0)
            , current_led(0)
            , total_leds(0)
            , leds_per_chunk(0)
            , stream_complete(false)
            , error_occurred(false)
            , pulses_per_bit(0)
        {}
    };

    /// @brief Initialize PARLIO peripheral on first use
    void initializeIfNeeded();

    /// @brief ISR callback for transmission completion (static wrapper)
    /// @note IRAM_ATTR applied in implementation (.cpp) to avoid section conflicts
    /// @note edata is const void* because parlio_tx_done_event_data_t is an ESP-IDF anonymous struct typedef
    static bool txDoneCallback(parlio_tx_unit_handle_t tx_unit,
                               const void* edata,
                               void* user_ctx);

    /// @brief Transpose and queue next chunk (called from ISR or main thread)
    /// @note IRAM_ATTR applied in implementation (.cpp) to avoid section conflicts
    bool transposeAndQueueNextChunk();

    /// @brief Initialization flag
    bool mInitialized;

    /// @brief PARLIO hardware state
    ParlioState mState;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr> mEnqueuedChannels;  ///< Channels enqueued via enqueue(), waiting for show()
    fl::vector<ChannelDataPtr> mTransmittingChannels;  ///< Channels currently transmitting (for cleanup)
};

//=============================================================================
// Polymorphic Wrapper Class
//=============================================================================

/// @brief Polymorphic PARLIO engine that auto-selects optimal data width implementation
///
/// This wrapper class contains five internal engine instances (1, 2, 4, 8, 16-bit)
/// and automatically delegates operations to the appropriate engine based on
/// the number of channels in each batch.
///
/// ## Architecture
/// - Contains five ChannelEnginePARLIOImpl instances (1, 2, 4, 8, 16-bit)
/// - Delegates to 1-bit engine for 1 channel
/// - Delegates to 2-bit engine for 2 channels
/// - Delegates to 4-bit engine for 3-4 channels
/// - Delegates to 8-bit engine for 5-8 channels
/// - Delegates to 16-bit engine for 9-16 channels
/// - Selection happens per-batch in beginTransmission()
/// - Inherits from IChannelEngine for standard FastLED integration
/// - Managed by ChannelBusManager which handles frame lifecycle events
///
/// ## Lifecycle
/// 1. **Construction**: Creates all 1/2/4/8/16-bit sub-engines
/// 2. **Each Frame**:
///    - enqueue() → batches channel data (inherited from ChannelEngine)
///    - show() → calls beginTransmission()
///    - beginTransmission() → selects engine based on channel count, delegates
/// 3. **Polling**: pollDerived() checks active engine's state
/// 4. **Destruction**: Cleans up all sub-engines
///
/// ## Memory Management
/// - Allocates all supported engines (1, 2, 4, 8, 16-bit)
/// - Total memory: ~37.2 KB (1-bit: ~1.2KB + 2-bit: ~2.4KB + 4-bit: ~4.8KB + 8-bit: ~9.6KB + 16-bit: ~19.2KB per buffer pair)
/// - Only active engine uses resources during transmission
/// - Engine selection optimizes memory usage per batch
///
/// @note Only available on ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 with PARLIO peripheral.
///       Compilation is guarded by FASTLED_ESP32_HAS_PARLIO feature flag.
class ChannelEnginePARLIO : public IChannelEngine {
public:
    /// @brief Constructor - creates all 1/2/4/8/16-bit sub-engines
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

private:
    /// @brief Begin LED data transmission with automatic engine selection
    /// @param channelData Span of channel data to transmit
    /// @note Selects optimal engine based on channel count (1, 2, 4, 8, or 16-bit)
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

private:
    /// @brief 1-bit sub-engine (handles 1 channel)
    ChannelEnginePARLIOImpl* mEngine1Bit;

    /// @brief 2-bit sub-engine (handles 2 channels)
    ChannelEnginePARLIOImpl* mEngine2Bit;

    /// @brief 4-bit sub-engine (handles 3-4 channels)
    ChannelEnginePARLIOImpl* mEngine4Bit;

    /// @brief 8-bit sub-engine (handles 5-8 channels)
    ChannelEnginePARLIOImpl* mEngine8Bit;

    /// @brief 16-bit sub-engine (handles 9-16 channels)
    ChannelEnginePARLIOImpl* mEngine16Bit;

    /// @brief Currently active engine (selected per-batch)
    ChannelEnginePARLIOImpl* mActiveEngine;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr> mEnqueuedChannels;  ///< Channels enqueued via enqueue(), waiting for show()
    fl::vector<ChannelDataPtr> mTransmittingChannels;  ///< Channels currently transmitting (for cleanup)
};

//=============================================================================
// Factory Function
//=============================================================================

/// @brief Create polymorphic PARLIO engine instance
/// @return Shared pointer to IChannelEngine, or nullptr if creation fails
/// @note Engine auto-selects optimal data width per batch (1, 2, 4, 8, or 16-bit)
fl::shared_ptr<IChannelEngine> createParlioEngine();

} // namespace fl

#endif // ESP32
