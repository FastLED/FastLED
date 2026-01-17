/// @file channel_engine_lcd_rgb.h
/// @brief LCD RGB implementation of ChannelEngine for ESP32-P4
///
/// This file implements a ChannelEngine that uses ESP32-P4's RGB LCD peripheral
/// to drive multiple WS2812/WS2812B LED strips in parallel via DMA.
///
/// ## Hardware Requirements
/// - ESP32-P4 (only variant with RGB LCD peripheral suitable for LED driving)
/// - 1-16 WS2812/WS2812B LED strips (parallel output via data bus)
/// - Configurable GPIO pins
/// - Optional PSRAM for large LED counts
///
/// ## Features
/// - **Multi-Channel Support**: Drive 1-16 LED strips simultaneously
/// - **DMA-Based Timing**: Hardware-generated precise WS2812 timing
/// - **4-Pixel Encoding**: Efficient waveform generation (8 bytes per bit)
/// - **Async Operation**: Non-blocking transmission with poll() state tracking
/// - **Double Buffering**: Seamless frame updates during transmission
/// - **Dependency Injection**: Mock peripheral support for unit testing
///
/// ## Usage Example
/// ```cpp
/// // ChannelEngineLcdRgb uses the IChannelEngine interface
/// CRGB leds[100];
/// void setup() {
///     FastLED.addLeds<WS2812, 1, GRB>(leds, 100);  // GPIO 1
///     // LCD RGB engine auto-selected on ESP32-P4
/// }
///
/// void loop() {
///     fill_rainbow(leds, 100, 0, 7);
///     FastLED.show();  // Non-blocking transmission via LCD RGB
/// }
/// ```
///
/// ## Performance Characteristics
/// - **Frame Rate**: 60+ FPS for typical LED counts (<500 LEDs per strip)
/// - **PCLK**: 3.2 MHz (optimized for WS2812 timing)
/// - **Memory Usage** (for 1000 LEDs × 8 strips):
///   - Scratch buffer: 24 KB (per-strip RGB data)
///   - DMA buffers: 2 × frame_size (double-buffered)
///
/// ## Technical Details
///
/// ### WS2812 Timing via LCD RGB
/// PCLK: 3.2 MHz (312.5ns per pixel)
/// - Bit 0: [HI, LO, LO, LO] = 312ns high, 938ns low
/// - Bit 1: [HI, HI, LO, LO] = 625ns high, 625ns low
/// - Each LED bit → 4 pixels × 2 bytes = 8 bytes
/// - Each RGB LED → 24 bits × 8 bytes = 192 bytes
///
/// ## Limitations
/// - **Platform-Specific**: Only available on ESP32-P4 with RGB LCD peripheral
/// - **Channel Count**: 1-16 channels supported (limited by LCD data bus width)
///
/// ## See Also
/// - Peripheral Interface: `ilcd_rgb_peripheral.h`
/// - ESP Implementation: `lcd_rgb_peripheral_esp.h`
/// - Mock Implementation: `lcd_rgb_peripheral_mock.h`

#pragma once

#include "fl/compiler_control.h"

#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"
#include "ilcd_rgb_peripheral.h"
#include "crgb.h"

namespace fl {

/// @brief Internal configuration structure for the LCD RGB channel engine
struct LcdRgbChannelEngineConfig {
    int pclk_gpio = -1;          ///< GPIO for pixel clock output
    int data_gpios[16];          ///< GPIO numbers for data lanes D0-D15
    int num_lanes = 0;           ///< Active lane count (1-16)
    bool use_psram = true;       ///< Allocate DMA buffers in PSRAM
};

/// @brief LCD RGB-based channel engine for parallel LED control on ESP32-P4
///
/// Implements IChannelEngine interface using ESP32-P4 LCD RGB peripheral for
/// LED data transmission. Uses dependency injection pattern for testability.
///
/// ## Architecture
/// - **Peripheral abstraction**: Uses ILcdRgbPeripheral* for hardware delegation
/// - **4-pixel encoding**: Each bit → 4 pixels (8 bytes per bit)
/// - **Multi-lane**: Parallel output on up to 16 data lines
/// - **State management**: Tracks enqueued/transmitting channels
/// - **Chipset grouping**: Groups channels by timing configuration
///
/// ## Lifecycle
/// 1. **Construction**: Inject ILcdRgbPeripheral* (real hardware or mock)
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
class ChannelEngineLcdRgb : public IChannelEngine {
public:
    /// @brief Constructor with dependency injection
    /// @param peripheral LCD RGB peripheral instance (real or mock)
    ///
    /// Stores a shared pointer to the peripheral to maintain proper lifetime.
    /// The peripheral will remain valid for the lifetime of this engine.
    explicit ChannelEngineLcdRgb(fl::shared_ptr<detail::ILcdRgbPeripheral> peripheral);

    /// @brief Destructor - waits for transmission completion
    ~ChannelEngineLcdRgb() override;

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
    /// @return "LCD_RGB"
    const char* getName() const override { return "LCD_RGB"; }

private:
    /// @brief Begin LED data transmission for current chipset group
    /// @param channelData Span of channel data to transmit
    /// @return true if transmission started successfully, false on error
    ///
    /// This method:
    /// 1. Validates channel data
    /// 2. Initializes LCD RGB peripheral if needed
    /// 3. Prepares scratch buffer with LED data
    /// 4. Encodes LED data to LCD RGB waveforms
    /// 5. Submits encoded data to LCD RGB peripheral
    bool beginTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief Prepare scratch buffer with per-lane data layout
    /// @param channelData Span of channel data to copy
    /// @param maxChannelSize Maximum channel size in bytes
    ///
    /// Copies LED RGB data from all channels into per-lane scratch buffer.
    void prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                              size_t maxChannelSize);

    /// @brief Encode frame data into DMA buffer
    ///
    /// Encodes LED RGB data from scratch buffer into 4-pixel waveform format.
    void encodeFrame();

private:
    /// @brief Group of channels sharing the same chipset timing
    struct ChipsetGroup {
        ChipsetTimingConfig mTiming;          ///< Shared timing configuration
        fl::vector<ChannelDataPtr> mChannels; ///< Channels in this group

        /// @brief Construct with timing config
        explicit ChipsetGroup(const ChipsetTimingConfig& timing)
            : mTiming(timing), mChannels() {}
    };

    /// @brief LCD RGB peripheral abstraction (injected dependency)
    fl::shared_ptr<detail::ILcdRgbPeripheral> mPeripheral;

    /// @brief Initialization flag
    bool mInitialized;

    /// @brief Driver configuration
    LcdRgbChannelEngineConfig mConfig;

    /// @brief Number of LEDs per strip
    int mNumLeds;

    /// @brief LED strip pointers for the driver
    CRGB* mStrips[16];

    /// @brief Scratch buffer for per-lane data layout (owned by channel engine)
    fl::vector<uint8_t> mScratchBuffer;

    /// @brief DMA buffers (double-buffered)
    uint16_t* mBuffers[2];
    size_t mBufferSize;
    int mFrontBuffer;

    /// @brief Internal state management for IChannelEngine interface
    fl::vector<ChannelDataPtr> mEnqueuedChannels;     ///< Channels waiting for show()
    fl::vector<ChannelDataPtr> mTransmittingChannels; ///< Channels currently transmitting

    /// @brief Chipset grouping state for sequential transmission
    fl::vector<ChipsetGroup> mChipsetGroups; ///< Groups of channels by timing
    size_t mCurrentGroupIndex;               ///< Index of currently transmitting group

    /// @brief Transfer state
    volatile bool mBusy;
    uint32_t mFrameCounter;
};

/// @brief Factory function to create LCD RGB engine with real hardware peripheral
/// @return Shared pointer to IChannelEngine, or nullptr if creation fails
///
/// Creates ChannelEngineLcdRgb with LcdRgbPeripheralEsp (real hardware).
/// Only available on ESP32-P4 with RGB LCD support.
fl::shared_ptr<IChannelEngine> createLcdRgbEngine();

} // namespace fl
