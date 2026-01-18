/// @file channel_engine_i2s.h
/// @brief I2S LCD_CAM implementation of ChannelEngine for ESP32-S3
///
/// This file implements a ChannelEngine that uses ESP32-S3's LCD_CAM peripheral
/// (via I2S mode) to drive multiple WS2812/WS2812B LED strips in parallel via DMA.
///
/// ## Hardware Requirements
/// - ESP32-S3 with LCD_CAM peripheral
/// - 1-16 WS2812/WS2812B LED strips (parallel output via data bus)
/// - Configurable GPIO pins
/// - PSRAM for large LED counts (recommended)
///
/// ## Features
/// - **Multi-Channel Support**: Drive 1-16 LED strips simultaneously
/// - **DMA-Based Timing**: Hardware-generated precise WS2812 timing
/// - **Transpose Encoding**: Efficient bit-level waveform generation (from Yves driver)
/// - **Async Operation**: Non-blocking transmission with poll() state tracking
/// - **Double Buffering**: Seamless frame updates during transmission
/// - **Dependency Injection**: Mock peripheral support for unit testing
///
/// ## Usage Example
/// ```cpp
/// // ChannelEngineI2S uses the IChannelEngine interface
/// CRGB leds[100];
/// void setup() {
///     FastLED.addLeds<WS2812, 1, GRB>(leds, 100);  // GPIO 1
///     // I2S engine auto-selected on ESP32-S3
/// }
///
/// void loop() {
///     fill_rainbow(leds, 100, 0, 7);
///     FastLED.show();  // Non-blocking transmission via I2S LCD_CAM
/// }
/// ```
///
/// ## Technical Details
///
/// ### WS2812 Timing via I2S LCD_CAM
/// PCLK: 2.4 MHz default (configurable)
/// - Bit encoding via transpose16x1_noinline2() from Yves driver
/// - Each RGB LED → 24 bits × 3 words = 72 bytes per LED (with encoding)
/// - 16-bit parallel output across all lanes simultaneously
///
/// ## Limitations
/// - **Platform-Specific**: Only available on ESP32-S3 with LCD_CAM peripheral
/// - **Channel Count**: 1-16 channels supported (limited by LCD data bus width)
/// - **Requires PSRAM**: Best performance with PSRAM enabled
///
/// ## See Also
/// - Peripheral Interface: `ii2s_lcd_cam_peripheral.h`
/// - ESP Implementation: `i2s_lcd_cam_peripheral_esp.h`
/// - Mock Implementation: `i2s_lcd_cam_peripheral_mock.h`
/// - Original Yves Driver: `third_party/yves/I2SClockLessLedDriveresp32s3/`

#pragma once

#include "fl/compiler_control.h"

#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"
#include "ii2s_lcd_cam_peripheral.h"
#include "crgb.h"

namespace fl {

/// @brief Internal configuration structure for the I2S channel engine
struct I2sChannelEngineConfig {
    int data_gpios[16];          ///< GPIO numbers for data lanes D0-D15
    int num_lanes = 0;           ///< Active lane count (1-16)
    uint32_t pclk_hz = 2400000;  ///< Pixel clock frequency (default 2.4 MHz)
    bool use_psram = true;       ///< Allocate DMA buffers in PSRAM
};

/// @brief I2S LCD_CAM-based channel engine for parallel LED control on ESP32-S3
///
/// Implements IChannelEngine interface using ESP32-S3 LCD_CAM peripheral for
/// LED data transmission. Uses dependency injection pattern for testability.
///
/// ## Architecture
/// - **Peripheral abstraction**: Uses II2sLcdCamPeripheral* for hardware delegation
/// - **Transpose encoding**: Efficient bit-parallel waveform generation
/// - **Multi-lane**: Parallel output on up to 16 data lines
/// - **State management**: Tracks enqueued/transmitting channels
/// - **Chipset grouping**: Groups channels by timing configuration
///
/// ## Lifecycle
/// 1. **Construction**: Inject II2sLcdCamPeripheral* (real hardware or mock)
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
class ChannelEngineI2S : public IChannelEngine {
public:
    /// @brief Constructor with dependency injection
    /// @param peripheral I2S LCD_CAM peripheral instance (real or mock)
    ///
    /// Stores a shared pointer to the peripheral to maintain proper lifetime.
    /// The peripheral will remain valid for the lifetime of this engine.
    explicit ChannelEngineI2S(fl::shared_ptr<detail::II2sLcdCamPeripheral> peripheral);

    /// @brief Destructor - waits for transmission completion
    ~ChannelEngineI2S() override;

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
    /// @return "I2S"
    const char* getName() const override { return "I2S"; }

private:
    /// @brief Begin LED data transmission for current chipset group
    /// @param channelData Span of channel data to transmit
    /// @return true if transmission started successfully, false on error
    ///
    /// This method:
    /// 1. Validates channel data
    /// 2. Initializes LCD_CAM peripheral if needed
    /// 3. Prepares scratch buffer with LED data
    /// 4. Encodes LED data to I2S waveforms (transpose encoding)
    /// 5. Submits encoded data to I2S LCD_CAM peripheral
    bool beginTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief Prepare scratch buffer with per-lane data layout
    /// @param channelData Span of channel data to copy
    /// @param maxChannelSize Maximum channel size in bytes
    ///
    /// Copies LED RGB data from all channels into per-lane scratch buffer.
    void prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                              size_t maxChannelSize);

    /// @brief Encode frame data into DMA buffer using transpose encoding
    ///
    /// Encodes LED RGB data from scratch buffer using the transpose algorithm
    /// from Yves driver (transpose16x1_noinline2 pattern).
    void encodeFrame();

    /// @brief Transpose 16x1 byte array for parallel output
    /// @param A Input byte array (16 bytes, one per lane)
    /// @param B Output word array (8 uint16_t words for bit-parallel output)
    static void transpose16x1(const uint8_t* A, uint16_t* B);

private:
    /// @brief Group of channels sharing the same chipset timing
    struct ChipsetGroup {
        ChipsetTimingConfig mTiming;          ///< Shared timing configuration
        fl::vector<ChannelDataPtr> mChannels; ///< Channels in this group

        /// @brief Construct with timing config
        explicit ChipsetGroup(const ChipsetTimingConfig& timing)
            : mTiming(timing), mChannels() {}
    };

    /// @brief I2S LCD_CAM peripheral abstraction (injected dependency)
    fl::shared_ptr<detail::II2sLcdCamPeripheral> mPeripheral;

    /// @brief Initialization flag
    bool mInitialized;

    /// @brief Driver configuration
    I2sChannelEngineConfig mConfig;

    /// @brief Number of LEDs per strip
    int mNumLeds;

    /// @brief Number of color components (3 for RGB, 4 for RGBW)
    int mNumComponents;

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

    /// @brief Wave8 expansion LUT for current timing configuration
    Wave8BitExpansionLut mWave8Lut;
    bool mWave8LutValid;
    ChipsetTimingConfig mCurrentTiming;
};

/// @brief Factory function to create I2S engine with real hardware peripheral
/// @return Shared pointer to IChannelEngine, or nullptr if creation fails
///
/// Creates ChannelEngineI2S with I2sLcdCamPeripheralEsp (real hardware).
/// Only available on ESP32-S3 with LCD_CAM support.
fl::shared_ptr<IChannelEngine> createI2sEngine();

} // namespace fl
