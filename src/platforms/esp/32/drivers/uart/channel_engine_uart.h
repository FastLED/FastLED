/// @file channel_engine_uart.h
/// @brief UART implementation of ChannelEngine for ESP32-C3/S3
///
/// This file implements a ChannelEngine that uses ESP32's UART peripheral to
/// drive WS2812/WS2812B LED strips on single GPIO pins. Unlike PARLIO which
/// supports multi-lane parallel output, UART is inherently serial (single-lane).
///
/// ## Hardware Requirements
/// - ESP32-C3, ESP32-S3, or other ESP32 variants with UART peripheral
/// - Single WS2812/WS2812B LED strip per UART instance
/// - Configurable GPIO pin for TX output
///
/// ## Features
/// - **Single-Lane Output**: One LED strip per UART peripheral
/// - **wave8-Adapted Encoding**: 2-bit LUT encoding optimized for UART framing
/// - **WS2812 Timing**: 3.2 Mbps UART achieves correct WS2812 timing
/// - **Async Operation**: Non-blocking transmission with poll() state tracking
/// - **No Transposition**: Single-lane architecture eliminates transposition overhead
/// - **Multi-UART Support**: Use multiple UART instances for parallel strips
///
/// ## Usage Example
/// ```cpp
/// // ChannelEngineUART is automatically registered with ChannelBusManager
/// // when enabled. Simply use FastLED's standard API:
///
/// CRGB leds[100];
/// void setup() {
///     FastLED.addLeds<WS2812, 17, GRB>(leds, 100);  // GPIO 17, UART1
/// }
///
/// void loop() {
///     fill_rainbow(leds, 100, 0, 7);
///     FastLED.show();  // Non-blocking transmission via UART
/// }
/// ```
///
/// ## Performance Characteristics
/// - **Frame Rate**: 60+ FPS for typical LED counts (<500 LEDs per strip)
/// - **Memory Usage** (for 1000 RGB LEDs):
///   - Scratch buffer: 3 KB (LED RGB data)
///   - UART buffer: 12 KB (wave8 encoded: 4:1 expansion)
///   - **Total: ~15 KB** per UART instance
/// - **CPU Overhead**: Minimal - encoding happens once, UART DMA handles transmission
///
/// ## Technical Details
///
/// ### WS2812 Timing via UART
/// UART baud rate: 3.2 Mbps (312.5ns per bit)
/// - LED bit 0: SHORT high, LONG low (via UART pattern 0x88/0x8C)
/// - LED bit 1: LONG high, SHORT low (via UART pattern 0xC8/0xCC)
/// - Each LED byte → 4 UART bytes (2-bit LUT encoding)
/// - Each RGB LED → 12 UART bytes total
///
/// ### UART Frame Structure (8N1)
/// Each UART byte becomes a 10-bit frame:
/// - 1 start bit (LOW) - automatic
/// - 8 data bits (from LUT)
/// - 1 stop bit (HIGH) - automatic
///
/// Automatic start/stop bits simplify waveform generation compared to
/// manual bit stuffing.
///
/// ### Encoding Algorithm
/// Uses 2-bit LUT (kUartEncode2BitLUT) to encode LED data:
/// ```
/// LED byte: 0xE4 (0b11100100)
///   Bits 7-6 (0b11) → 0xCC
///   Bits 5-4 (0b10) → 0xC8
///   Bits 3-2 (0b01) → 0x8C
///   Bits 1-0 (0b00) → 0x88
/// Result: [0xCC, 0xC8, 0x8C, 0x88] (4 UART bytes)
/// ```
///
/// ### Buffer Size Calculation
/// Formula: buffer_size = num_leds × 3 bytes/LED × 4 expansion = num_leds × 12 bytes
///
/// Examples:
/// - 100 RGB LEDs: 100 × 12 = 1,200 bytes
/// - 500 RGB LEDs: 500 × 12 = 6,000 bytes
/// - 1000 RGB LEDs: 1000 × 12 = 12,000 bytes
///
/// ## Limitations
/// - **Single-Lane**: Each UART peripheral drives only one LED strip
/// - **Multiple UARTs for Parallel**: Use UART0, UART1, UART2 for up to 3 strips
/// - **Platform-Specific**: Available on all ESP32 variants with UART peripheral
/// - **Timing Constraints**: Baud rate must match LED protocol (3.2 Mbps for WS2812)
///
/// ## See Also
/// - Implementation: `channel_engine_uart.cpp`
/// - Unit Tests: `tests/platforms/esp/32/drivers/uart/test_uart_channel_engine.cpp`
/// - Peripheral Interface: `iuart_peripheral.h`
/// - Encoding: `wave8_encoder_uart.h`

#pragma once

#include "fl/compiler_control.h"

#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/chipsets/led_timing.h"
#include "fl/engine_events.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "iuart_peripheral.h"
#include "wave8_encoder_uart.h"

namespace fl {

/// @brief UART-based channel engine for single-lane LED control
///
/// Implements IChannelEngine interface using ESP32 UART peripheral for
/// LED data transmission. Uses dependency injection pattern for testability.
///
/// ## Architecture
/// - **Peripheral abstraction**: Uses IUartPeripheral* for hardware delegation
/// - **wave8 encoding**: Encodes LED data using 2-bit LUT (4:1 expansion)
/// - **Single-lane**: No transposition needed (UART is inherently serial)
/// - **State management**: Tracks enqueued/transmitting channels
/// - **Chipset grouping**: Groups channels by timing configuration
///
/// ## Lifecycle
/// 1. **Construction**: Inject IUartPeripheral* (real hardware or mock)
/// 2. **Enqueue**: User calls enqueue() to add channels
/// 3. **Show**: User calls show() to trigger transmission
/// 4. **Poll**: User polls poll() to check transmission state
/// 5. **Cleanup**: Destructor waits for completion and releases resources
///
/// ## State Machine
/// ```
/// READY → enqueue() → READY (accumulating channels)
/// READY → show() → BUSY (encoding + transmission start)
/// BUSY → poll() → DRAINING (transmission in progress)
/// DRAINING → poll() → READY (transmission complete)
/// ```
class ChannelEngineUART : public IChannelEngine {
public:
    /// @brief Constructor with dependency injection
    /// @param peripheral UART peripheral instance (real or mock)
    ///
    /// Stores a shared pointer to the peripheral to maintain proper lifetime.
    /// The peripheral will remain valid for the lifetime of this engine.
    explicit ChannelEngineUART(fl::shared_ptr<IUartPeripheral> peripheral);

    /// @brief Destructor - waits for transmission completion
    ~ChannelEngineUART() override;

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    ///
    /// Adds channel to internal queue. Transmission happens when show() is called.
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    ///
    /// Groups channels by timing configuration and begins transmission of
    /// first group. Subsequent groups transmit sequentially via poll().
    void show() override;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    ///
    /// Must be called regularly to:
    /// - Check transmission completion status
    /// - Trigger sequential chipset group transmissions
    /// - Clean up completed transmissions
    EngineState poll() override;

    /// @brief Get the engine name for affinity binding
    /// @return "UART"
    const char* getName() const override { return "UART"; }

private:
    /// @brief Begin LED data transmission for current chipset group
    /// @param channelData Span of channel data to transmit
    ///
    /// This method:
    /// 1. Validates channel data
    /// 2. Initializes UART peripheral if needed
    /// 3. Prepares scratch buffer with LED data
    /// 4. Encodes LED data to UART bytes using wave8 encoding
    /// 5. Submits encoded data to UART peripheral
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief Prepare scratch buffer with per-lane data layout
    /// @param channelData Span of channel data to copy
    /// @param maxChannelSize Maximum channel size in bytes
    ///
    /// Copies LED RGB data from all channels into linear scratch buffer.
    /// UART is single-lane so no transposition is needed.
    void prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                              size_t maxChannelSize);

private:
    /// @brief Group of channels sharing the same chipset timing
    struct ChipsetGroup {
        ChipsetTimingConfig mTiming;          ///< Shared timing configuration
        fl::vector<ChannelDataPtr> mChannels; ///< Channels in this group
    };

    /// @brief UART peripheral abstraction (injected dependency)
    /// Stored as shared_ptr to maintain proper lifetime management
    fl::shared_ptr<IUartPeripheral> mPeripheral;

    /// @brief Initialization flag
    bool mInitialized;

    /// @brief Scratch buffer for LED RGB data (owned by channel engine)
    fl::vector<uint8_t> mScratchBuffer;

    /// @brief Encoded UART buffer for wave8 output (owned by channel engine)
    fl::vector<uint8_t> mEncodedBuffer;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr> mEnqueuedChannels;     ///< Channels waiting for show()
    fl::vector<ChannelDataPtr> mTransmittingChannels; ///< Channels currently transmitting

    /// @brief Chipset grouping state for sequential transmission
    fl::vector<ChipsetGroup> mChipsetGroups; ///< Groups of channels by timing
    size_t mCurrentGroupIndex;               ///< Index of currently transmitting group
};

/// @brief Factory function to create UART engine with real hardware peripheral
/// @param uart_num UART peripheral number (0, 1, or 2)
/// @param tx_pin GPIO pin for TX output
/// @param baud_rate UART baud rate (typically 3200000 for WS2812)
/// @return Shared pointer to IChannelEngine, or nullptr if creation fails
///
/// Creates ChannelEngineUART with UartPeripheralEsp (real hardware).
/// The engine will be registered with ChannelBusManager automatically.
fl::shared_ptr<IChannelEngine> createUartEngine(int uart_num,
                                                int tx_pin,
                                                uint32_t baud_rate = 3200000);

} // namespace fl
