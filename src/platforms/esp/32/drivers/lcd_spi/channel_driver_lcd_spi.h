/// @file channel_driver_lcd_spi.h
/// @brief LCD_CAM SPI channel driver for true SPI chipsets (APA102, SK9822)
///
/// Uses LCD_CAM I80 bus on ESP32-S3 to drive clocked SPI LED strips.
/// Handles SPI chipsets only (not clockless). Data is transposed for
/// parallel 16-bit output on the I80 bus.
///
/// ## ISR-Driven Chunked DMA Streaming
///
/// The driver splits LED data into fixed-size chunks and transmits them
/// via a 3-buffer ring. When a DMA chunk completes, the ISR callback
/// transposes the next chunk (~1-2 µs) and submits it immediately.
/// This reduces peak memory from O(total_leds) to O(chunk_size × 3)
/// and enables arbitrarily long strips without OOM.
///
/// See issue #2258 for design details and PARLIO reference.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/lcd_spi/ilcd_spi_peripheral.h"

namespace fl {

/// @brief Channel driver for SPI chipsets via LCD_CAM I80 bus
///
/// Implements IChannelDriver for true SPI protocols (APA102, SK9822, HD108).
/// Uses LCD_CAM peripheral with PCLK as SPI clock and D0-D15 as parallel
/// data lanes. Data bytes are transposed into 16-bit words for I80 output.
///
/// Transmission is ISR-driven: show() kicks off the first DMA chunk,
/// then the ISR callback handles subsequent chunks automatically.
/// poll() reports DRAINING until all chunks complete.
class ChannelDriverLcdSpi : public IChannelDriver {
  public:
    explicit ChannelDriverLcdSpi(
        fl::shared_ptr<detail::ILcdSpiPeripheral> peripheral) FL_NOEXCEPT;
    ~ChannelDriverLcdSpi() override;

    bool canHandle(const ChannelDataPtr &data) const FL_NOEXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT override;
    void show() FL_NOEXCEPT override;
    DriverState poll() FL_NOEXCEPT override;

    fl::string getName() const FL_NOEXCEPT override {
        return fl::string::from_literal("LCD_SPI");
    }

    Capabilities getCapabilities() const FL_NOEXCEPT override {
        return Capabilities(false, true); // SPI only
    }

    //=========================================================================
    // Ring buffer configuration (public for testing)
    //=========================================================================

    static constexpr size_t kRingBufferCount = 3;

    /// Default source-byte capacity per ring buffer slot.
    /// Each source byte produces 8 u16 words (16 bytes) in the DMA buffer.
    /// Capped at ~100 APA102 LEDs (4+100*4+4 = 408 bytes → ~6.5KB DMA/slot).
    static constexpr size_t kDefaultChunkInputBytes = 408;

    /// Override chunk input bytes for testing (0 = use default).
    void setChunkInputBytesForTest(size_t bytes) FL_NOEXCEPT {
        mChunkInputBytesOverride = bytes;
    }

  private:
    bool beginTransmission(
        fl::span<const ChannelDataPtr> channels) FL_NOEXCEPT;

    /// @brief Transpose a range of source bytes into 16-bit words
    /// @param channels Source channel data spans
    /// @param output DMA buffer to write transposed words into
    /// @param startByte First source byte index to transpose
    /// @param byteCount Number of source bytes to transpose
    void transposeToWords(fl::span<const ChannelDataPtr> channels,
                          u16 *output, size_t startByte,
                          size_t byteCount) FL_NOEXCEPT;

    /// @brief Free all ring buffer allocations
    void freeRingBuffers() FL_NOEXCEPT;

    /// @brief Allocate ring buffers with given per-slot capacity
    bool allocateRingBuffers(size_t slotCapacityBytes) FL_NOEXCEPT;

    //=========================================================================
    // ISR callback — called when a DMA chunk completes
    //=========================================================================

    /// @brief ISR callback: transpose and submit next chunk, or mark complete.
    ///
    /// On ESP32: runs in ISR context (IRAM_ATTR). Must be <10 µs.
    /// On host (mock): runs synchronously from simulateTransmitComplete().
    ///
    /// @param panel_io ESP panel IO handle (nullptr on mock)
    /// @param edata ESP event data (nullptr on mock)
    /// @param user_ctx Pointer to ChannelDriverLcdSpi instance
    /// @return false (no higher-priority task woken by this callback)
    static bool isrChunkDone(void *panel_io, const void *edata,
                             void *user_ctx) FL_NOEXCEPT;

    //=========================================================================
    // ISR context — coordinates chunked streaming between ISR and main
    //=========================================================================

    /// @brief Stream state for ISR-driven chunked DMA
    ///
    /// Memory model (follows PARLIO's parlio_isr_context.h):
    /// - ISR writes volatile fields (mStreamComplete, mNextByteOffset, etc.)
    /// - Main thread reads volatile fields directly
    /// - After detecting mStreamComplete==true, main reads non-volatile fields
    struct IsrContext {
        // Volatile: ISR writes, main reads
        volatile bool mStreamComplete;
        volatile size_t mNextByteOffset; // next source byte to transpose
        volatile size_t mRingWriteIdx;   // next ring slot to write (0-2)

        // Non-volatile: set once before streaming starts
        size_t mTotalBytes;       // max source bytes across all channels
        size_t mChunkInputBytes;  // source bytes per chunk

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

    // Ring buffer: 3 DMA buffers for chunked streaming
    u16 *mRingBuffers[kRingBufferCount];
    size_t mRingCapacity; // DMA bytes per ring slot

    int mNumLanes;
    volatile bool mBusy;
    IsrContext mIsrCtx;
    size_t mChunkInputBytesOverride; // 0 = use kDefaultChunkInputBytes
};

/// @brief Factory function to create LCD_SPI driver with real hardware
fl::shared_ptr<IChannelDriver> createLcdSpiEngine() FL_NOEXCEPT;

} // namespace fl
