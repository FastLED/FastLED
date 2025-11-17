/// @file channel_engine_parlio.h
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/S3
///
/// This file implements a ChannelEngine that uses ESP32's Parallel IO (PARLIO) peripheral
/// to drive multiple WS2812/WS2812B LED strips simultaneously on parallel GPIO pins.
///
/// ## Hardware Requirements
/// - ESP32-P4 or ESP32-S3 (only variants with PARLIO peripheral)
/// - Exactly 8 WS2812/WS2812B LED strips (8 lanes only)
/// - Configurable GPIO pins (default: GPIO 1-8)
///
/// ## Features
/// - **Parallel Transmission**: Drive 8 LED strips simultaneously with zero CPU overhead
/// - **WS2812 Timing**: Accurate 3.2 MHz clock with 4-tick encoding (312.5ns per tick)
/// - **Async Operation**: Non-blocking transmission with pollDerived() state tracking
/// - **Large LED Support**: Automatic chunking for >682 LEDs per strip
/// - **ISR Transposition**: Direct waveform encoding and bit transposition in ISR callback
/// - **ISR Streaming**: Memory-safe ping-pong buffering with owned channel pointer storage
///
/// ## Usage Example
/// ```cpp
/// // ChannelEnginePARLIO is automatically registered when FASTLED_ESP32_HAS_PARLIO is enabled
/// // Simply use FastLED's standard API:
///
/// CRGB leds[100];
/// void setup() {
///     FastLED.addLeds<WS2812, 1, GRB>(leds, 100);  // GPIO 1
///     // PARLIO engine handles the rest automatically
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
/// ## Limitations
/// - **Platform-Specific**: Only available on ESP32-P4 and ESP32-S3 with PARLIO peripheral
/// - **Channel Count**: Exactly 8 channels required
///   - 1-7 channels: Not supported
///   - 9+ channels: Not supported
///   - Driver will reject with clear error message for non-8-channel configurations
/// - **Fixed Pins**: Currently uses hardcoded GPIO 1-8 (future: user-configurable)
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

/// @brief Parallel IO-based ChannelEngine implementation for ESP32-P4/S3
///
/// This class implements the ChannelEngine interface using ESP32's PARLIO peripheral
/// to drive multiple WS2812/WS2812B LED strips simultaneously in parallel.
///
/// ## Architecture
/// - Inherits from ChannelEngine for standard FastLED integration
/// - Implements EngineEvents::Listener to receive frame completion events
/// - Uses PARLIO TX unit for hardware-accelerated parallel transmission
/// - **ISR-based streaming**: Ping-pong buffering with automatic DMA refilling
/// - Maintains internal state for async operation (transmitting flag, double buffers, streaming position)
///
/// ## Lifecycle
/// 1. **Construction**: Registers as EngineEvents listener
/// 2. **First Transmission**: Lazy initialization of PARLIO peripheral (double buffers, ISR callbacks)
/// 3. **Each Frame**:
///    - beginTransmission() → transposes first 2 chunks into ping-pong buffers
///    - DMA starts transmitting first chunk
///    - ISR callback auto-transposes and queues remaining chunks
/// 4. **Polling**: pollDerived() checks streaming status (non-blocking, ISR-driven)
/// 5. **Destruction**: Waits for active transmission, cleans up hardware resources
///
/// ## Thread Safety
/// - Not thread-safe - expected to be called from single thread (Arduino loop)
/// - Uses volatile flag for transmission state (polling from main loop)
/// - Hardware synchronization via PARLIO driver's wait functions
///
/// ## Memory Management
/// - **Ping-pong double buffering**: Two fixed-size DMA-capable buffers
/// - **Buffer size**: Independent of total LED count, determined by chunk size (default: 100 LEDs)
/// - **Fixed allocation**: ~19.2 KB for 8-bit width (2 × 100 LEDs × 96 bytes)
/// - **Streaming architecture**: Supports arbitrary LED counts without memory scaling
/// - **ISR transposition**: Data is transposed on-the-fly during transmission, not upfront
///
/// @note Only available on ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 with PARLIO peripheral.
///       ESP32-S3 does NOT have PARLIO hardware (uses LCD peripheral instead).
///       Compilation is guarded by FASTLED_ESP32_HAS_PARLIO feature flag.
class ChannelEnginePARLIO : public ChannelEngine, public EngineEvents::Listener {
public:
    ChannelEnginePARLIO();
    ~ChannelEnginePARLIO() override;

    // EngineEvents::Listener interface
    void onEndFrame() override;

protected:
    /// @brief Query engine state (hardware polling implementation)
    /// @return Current engine state (READY, BUSY, or ERROR)
    EngineState pollDerived() override;

    /// @brief Begin LED data transmission for all channels
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData) override;

private:
    /// @brief PARLIO hardware state with ISR-based streaming support
    struct ParlioState {
        parlio_tx_unit_handle_t tx_unit;     ///< PARLIO TX unit handle
        fl::vector<int> pins;                 ///< GPIO pin assignments (gpio_num_t cast to int) - always 8 pins
        volatile bool transmitting;           ///< Transmission in progress flag

        // Double-buffered DMA streaming (DMA-capable memory for waveform output)
        uint8_t* buffer_a;                    ///< First DMA buffer (ping-pong, heap_caps_malloc)
        uint8_t* buffer_b;                    ///< Second DMA buffer (ping-pong, heap_caps_malloc)
        size_t buffer_size;                   ///< Size of each DMA buffer in bytes
        volatile uint8_t* active_buffer;      ///< Currently transmitting buffer
        volatile uint8_t* fill_buffer;        ///< Buffer being filled

        // Streaming state
        volatile size_t num_lanes;            ///< Number of parallel lanes (channels)
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
        fl::vector<uint8_t> scratch_padded_buffer; ///< Contiguous buffer for all N lanes (regular heap, non-DMA)

        // Precomputed waveforms (generated from timing parameters during initialization)
        fl::array<uint8_t, 4> bit0_waveform;      ///< Waveform pattern for bit 0 (4 pulses)
        fl::array<uint8_t, 4> bit1_waveform;      ///< Waveform pattern for bit 1 (4 pulses)
        size_t pulses_per_bit;                     ///< Number of pulses per bit (should be 4 for WS2812)

        ParlioState()
            : tx_unit(nullptr)
            , transmitting(false)
            , buffer_a(nullptr)
            , buffer_b(nullptr)
            , buffer_size(0)
            , active_buffer(nullptr)
            , fill_buffer(nullptr)
            , num_lanes(0)
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
};

} // namespace fl

#endif // ESP32
