/// @file channel_driver_lcd_clockless.h
/// @brief LCD_CAM I80 channel driver for clockless LED chipsets (WS2812, SK6812)
///
/// Uses LCD_CAM I80 bus on ESP32-S3 to drive clockless LED strips.
/// Handles clockless chipsets only (not SPI). Data is encoded using
/// wave3 or wave8 waveform expansion and transposed for parallel
/// 16-bit output on the I80 bus.
///
/// This driver replaces the misnamed "I2S" driver on ESP32-S3 (which
/// actually wraps the LCD_CAM peripheral, not I2S). The "I2S" name
/// is retained only for ESP32dev which has a real I2S peripheral.
///
/// ## Encoding Auto-Selection
///
/// - wave3 (3 ticks/bit): Used when canUseWave3(timing) returns true.
///   Saves 2.67x memory vs wave8. Works for WS2812, SK6812, etc.
/// - wave8 (8 ticks/bit): Fallback for chipsets that need finer timing.
///
/// ## ISR-Driven Chunked DMA
///
/// Same architecture as ChannelDriverLcdSpi — 3-buffer ring, ISR callback
/// transposes next chunk (~1-2 µs) and submits immediately.
/// See issue #2258 for design details.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/wave3.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/lcd_spi/ilcd_spi_peripheral.h"

namespace fl {

/// @brief Channel driver for clockless chipsets via LCD_CAM I80 bus
///
/// Implements IChannelDriver for clockless protocols (WS2812, SK6812, etc.).
/// Uses LCD_CAM peripheral with PCLK providing pulse timing and D0-D15 as
/// parallel data lanes. Data bytes are wave-encoded and transposed into
/// 16-bit words for I80 output.
class ChannelDriverLcdClockless : public IChannelDriver {
  public:
    explicit ChannelDriverLcdClockless(
        fl::shared_ptr<detail::ILcdSpiPeripheral> peripheral) FL_NOEXCEPT;
    ~ChannelDriverLcdClockless() override;

    bool canHandle(const ChannelDataPtr &data) const FL_NOEXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT override;
    void show() FL_NOEXCEPT override;
    DriverState poll() FL_NOEXCEPT override;

    fl::string getName() const FL_NOEXCEPT override {
        return fl::string::from_literal("LCD_CLOCKLESS");
    }

    Capabilities getCapabilities() const FL_NOEXCEPT override {
        return Capabilities(true, false); // clockless only
    }

    //=========================================================================
    // Ring buffer configuration
    //=========================================================================

    static constexpr size_t kRingBufferCount = 3;

    /// Default: ~30 LEDs per chunk for clockless (3 bytes/LED).
    /// wave3: 90 bytes * 48 = 4.3KB per DMA slot → 13KB total ring.
    /// wave8: 90 bytes * 128 = 11.5KB per DMA slot → 34.5KB total ring.
    static constexpr size_t kDefaultChunkInputBytes = 90;

    /// Override chunk input bytes for testing (0 = use default).
    void setChunkInputBytesForTest(size_t bytes) FL_NOEXCEPT {
        mChunkInputBytesOverride = bytes;
    }

  private:
    bool beginTransmission(
        fl::span<const ChannelDataPtr> channels) FL_NOEXCEPT;

    /// @brief Encode and transpose a range of source bytes using wave3 or wave8
    /// @param channels Source channel data spans
    /// @param output DMA buffer (u16 words)
    /// @param startByte First source byte index
    /// @param byteCount Number of source bytes to encode
    /// @return Number of DMA bytes written
    size_t encodeChunk(fl::span<const ChannelDataPtr> channels,
                       u16 *output, size_t startByte,
                       size_t byteCount) FL_NOEXCEPT;

    void freeRingBuffers() FL_NOEXCEPT;
    bool allocateRingBuffers(size_t slotCapacityBytes) FL_NOEXCEPT;

    //=========================================================================
    // ISR callback
    //=========================================================================

    static bool isrChunkDone(void *panel_io, const void *edata,
                             void *user_ctx) FL_NOEXCEPT;

    //=========================================================================
    // ISR context
    //=========================================================================

    struct IsrContext {
        volatile bool mStreamComplete;
        volatile size_t mNextByteOffset;
        volatile size_t mRingWriteIdx;

        size_t mTotalBytes;
        size_t mChunkInputBytes;

        void reset() FL_NOEXCEPT {
            mStreamComplete = false;
            mNextByteOffset = 0;
            mRingWriteIdx = 0;
            mTotalBytes = 0;
            mChunkInputBytes = 0;
        }
    };

    //=========================================================================
    // Member state
    //=========================================================================

    fl::shared_ptr<detail::ILcdSpiPeripheral> mPeripheral;
    bool mInitialized;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    u16 *mRingBuffers[kRingBufferCount];
    size_t mRingCapacity;

    int mNumLanes;
    volatile bool mBusy;
    IsrContext mIsrCtx;
    size_t mChunkInputBytesOverride;

    // Encoding state (set during beginTransmission, stable during ISR)
    bool mUseWave3;
    Wave3BitExpansionLut mWave3Lut;
    Wave8BitExpansionLut mWave8Lut;
    u32 mClockHz;
    size_t mOutputBytesPerInputByte; // 48 for wave3, 128 for wave8 (with 16 lanes)
};

/// @brief Factory function to create LCD clockless driver
fl::shared_ptr<IChannelDriver> createLcdClocklessEngine() FL_NOEXCEPT;

} // namespace fl
