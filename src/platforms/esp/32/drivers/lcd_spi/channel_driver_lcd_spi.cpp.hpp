// IWYU pragma: private

/// @file channel_driver_lcd_spi.cpp
/// @brief LCD_CAM SPI channel driver implementation
///
/// ISR-driven chunked DMA streaming for SPI LED chipsets (APA102, SK9822).
/// See channel_driver_lcd_spi.h and issue #2258 for architecture details.

#include "platforms/is_platform.h"

// Compile for ESP32-S3 (with LCD_CAM) or host testing
#if defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) &&          \
     (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))
#define FL_LCD_SPI_COMPILE 1
#elif defined(FL_IS_ESP32)
#include "sdkconfig.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_LCD_SPI
#define FL_LCD_SPI_COMPILE 1
#endif
#endif

#ifdef FL_LCD_SPI_COMPILE

#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.h"
#include "fl/system/log.h"
#include "fl/task/executor.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

// Include ESP peripheral for factory function
#if defined(FL_IS_ESP_32S3)
#include "fl/stl/has_include.h"
#if FL_HAS_INCLUDE("esp_lcd_panel_io.h")
// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_esp.h"
// IWYU pragma: end_keep
#define FL_LCD_SPI_HAS_ESP_PERIPHERAL 1
#endif
#endif
#ifndef FL_LCD_SPI_HAS_ESP_PERIPHERAL
#define FL_LCD_SPI_HAS_ESP_PERIPHERAL 0
#endif

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelDriverLcdSpi::ChannelDriverLcdSpi(
    fl::shared_ptr<detail::ILcdSpiPeripheral> peripheral) FL_NOEXCEPT
    : mPeripheral(fl::move(peripheral)), mInitialized(false),
      mEnqueuedChannels(), mTransmittingChannels(),
      mRingBuffers{nullptr, nullptr, nullptr}, mRingCapacity(0),
      mNumLanes(0), mBusy(false), mIsrCtx(), mChunkInputBytesOverride(0) {
    mIsrCtx.reset();
}

ChannelDriverLcdSpi::~ChannelDriverLcdSpi() {
    // Block until DMA completes — ring buffers are DMA sources,
    // so freeing them while DMA is active would be a use-after-free.
    if (mBusy && mPeripheral) {
        bool done = mPeripheral->waitTransmitDone(2000);
        mBusy = false;
        if (!done) {
            // If a 2 s wait didn't drain the peripheral, it's either stuck
            // hardware or a mock with no auto-completion. Free anyway:
            // LeakSanitizer flags the alternative, and a 2 s stall already
            // indicates the DMA is lost.
            FL_WARN("ChannelDriverLcdSpi: DMA wait timed out — "
                    "freeing ring buffers anyway");
        }
    }
    freeRingBuffers();
}

//=============================================================================
// Ring Buffer Management
//=============================================================================

void ChannelDriverLcdSpi::freeRingBuffers() FL_NOEXCEPT {
    for (size_t i = 0; i < kRingBufferCount; i++) {
        if (mPeripheral && mRingBuffers[i] != nullptr) {
            mPeripheral->freeBuffer(mRingBuffers[i]);
            mRingBuffers[i] = nullptr;
        }
    }
    mRingCapacity = 0;
}

bool ChannelDriverLcdSpi::allocateRingBuffers(
    size_t slotCapacityBytes) FL_NOEXCEPT {
    if (mRingCapacity >= slotCapacityBytes && mRingBuffers[0] != nullptr) {
        return true; // already big enough
    }

    freeRingBuffers();

    for (size_t i = 0; i < kRingBufferCount; i++) {
        mRingBuffers[i] = mPeripheral->allocateBuffer(slotCapacityBytes);
        if (mRingBuffers[i] == nullptr) {
            FL_WARN("ChannelDriverLcdSpi: ring buffer alloc failed slot "
                    << i);
            freeRingBuffers();
            return false;
        }
    }
    mRingCapacity = slotCapacityBytes;
    return true;
}

//=============================================================================
// IChannelDriver Interface
//=============================================================================

bool ChannelDriverLcdSpi::canHandle(
    const ChannelDataPtr &data) const FL_NOEXCEPT {
    if (!data) {
        return false;
    }
    return data->isSpi();
}

void ChannelDriverLcdSpi::enqueue(ChannelDataPtr channelData) FL_NOEXCEPT {
    mEnqueuedChannels.push_back(fl::move(channelData));
}

void ChannelDriverLcdSpi::show() FL_NOEXCEPT {
    // Release channels from any completed previous transmission
    poll();

    if (mEnqueuedChannels.empty()) {
        return;
    }

    // Wait for previous transmission to complete (max ~5 seconds)
    constexpr int kMaxIterations = 20000;
    int iterations = 0;
    while (mBusy && iterations++ < kMaxIterations) {
        poll();
        fl::task::run(250, fl::task::ExecFlags::SYSTEM);
    }
    if (mBusy) {
        FL_WARN("ChannelDriverLcdSpi: DMA hung — forcing release");
        mBusy = false;
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }

    mTransmittingChannels = fl::move(mEnqueuedChannels);

    if (!beginTransmission(mTransmittingChannels)) {
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }
}

IChannelDriver::DriverState ChannelDriverLcdSpi::poll() FL_NOEXCEPT {
    if (mTransmittingChannels.empty()) {
        return DriverState::READY;
    }

    // Stream complete: ISR processed all chunks and peripheral is idle
    bool streamDone = mIsrCtx.mStreamComplete;
    bool peripheralIdle = mPeripheral && !mPeripheral->isBusy();

    if (streamDone || peripheralIdle) {
        mBusy = false;
        mIsrCtx.mStreamComplete = false;
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
        return DriverState::READY;
    }

    return mBusy ? DriverState::DRAINING : DriverState::READY;
}

//=============================================================================
// Transposition (byte → 16-bit word, per-chunk)
//=============================================================================

void ChannelDriverLcdSpi::transposeToWords(
    fl::span<const ChannelDataPtr> channels, u16 *output,
    size_t startByte, size_t byteCount) FL_NOEXCEPT {
    // For each byte position in [startByte, startByte+byteCount),
    // create 8 u16 words (one per bit, MSB first).
    // Each bit N of the output word = corresponding bit of byte from lane N.
    for (size_t byteIdx = startByte; byteIdx < startByte + byteCount;
         byteIdx++) {
        u8 lane_bytes[16];
        fl::memset(lane_bytes, 0, sizeof(lane_bytes));

        for (size_t lane = 0; lane < channels.size() && lane < 16; lane++) {
            const auto &data = channels[lane]->getData();
            if (byteIdx < data.size()) {
                lane_bytes[lane] = data[byteIdx];
            }
        }

        for (int bit = 7; bit >= 0; bit--) {
            u16 word = 0;
            for (size_t lane = 0; lane < channels.size() && lane < 16;
                 lane++) {
                if (lane_bytes[lane] & (1 << bit)) {
                    word |= (1 << lane);
                }
            }
            *output++ = word;
        }
    }
}

//=============================================================================
// ISR Callback — processes chunk completion
//=============================================================================

bool ChannelDriverLcdSpi::isrChunkDone(void *panel_io, const void *edata,
                                       void *user_ctx) FL_NOEXCEPT {
    (void)panel_io;
    (void)edata;

    auto *self = static_cast<ChannelDriverLcdSpi *>(user_ctx);
    IsrContext &ctx = self->mIsrCtx;

    // All chunks transmitted?
    if (ctx.mNextByteOffset >= ctx.mTotalBytes) {
        ctx.mStreamComplete = true;
        self->mBusy = false;
        return false;
    }

    // Transpose next chunk into the next ring buffer slot
    size_t writeIdx = ctx.mRingWriteIdx;
    u16 *buf = self->mRingBuffers[writeIdx];

    // Calculate bytes for this chunk (may pad beyond source data — that's OK,
    // transposeToWords outputs zeros for out-of-range bytes)
    size_t bytesRemaining = ctx.mTotalBytes - ctx.mNextByteOffset;
    size_t chunkBytes = ctx.mChunkInputBytes;
    if (chunkBytes > bytesRemaining) {
        chunkBytes = bytesRemaining;
    }

    fl::memset(buf, 0, chunkBytes * 8 * sizeof(u16));
    self->transposeToWords(self->mTransmittingChannels, buf,
                           ctx.mNextByteOffset, chunkBytes);

    ctx.mNextByteOffset += chunkBytes;
    ctx.mRingWriteIdx = (writeIdx + 1) % kRingBufferCount;

    // Submit this chunk for DMA
    size_t dmaBytes = chunkBytes * 8 * sizeof(u16);
    if (!self->mPeripheral->transmit(buf, dmaBytes)) {
        // Transmit failed — abort stream
        ctx.mStreamComplete = true;
        self->mBusy = false;
        FL_WARN("ChannelDriverLcdSpi: ISR transmit failed");
    }

    return false;
}

//=============================================================================
// Begin Transmission — sets up chunked streaming
//=============================================================================

bool ChannelDriverLcdSpi::beginTransmission(
    fl::span<const ChannelDataPtr> channels) FL_NOEXCEPT {
    if (channels.empty() || !mPeripheral) {
        return false;
    }

    // Find max data size across all channels
    size_t maxSize = 0;
    for (const auto &channel : channels) {
        size_t sz = channel->getSize();
        if (sz > maxSize) {
            maxSize = sz;
        }
    }

    if (maxSize == 0) {
        return false;
    }

    int numLanes = static_cast<int>(channels.size());

    // Determine chunk size
    size_t chunkInputBytes =
        mChunkInputBytesOverride > 0 ? mChunkInputBytesOverride
                                     : kDefaultChunkInputBytes;
    if (chunkInputBytes > maxSize) {
        chunkInputBytes = maxSize;
    }

    // Each source byte → 8 u16 words → 16 DMA bytes
    size_t slotCapacityBytes = chunkInputBytes * 8 * sizeof(u16);

    // Only reinitialize the peripheral when hardware config changes
    // (lane count). This avoids tearing down the I80 bus GPIO matrix.
    bool needPeripheralInit = !mInitialized || (numLanes != mNumLanes);

    if (needPeripheralInit) {
        freeRingBuffers();

        if (mInitialized) {
            mPeripheral->deinitialize();
            mInitialized = false;
        }

        detail::LcdSpiConfig config;
        config.num_lanes = numLanes;
        // max_transfer_bytes = chunk DMA size (all chunks are uniform)
        config.max_transfer_bytes = slotCapacityBytes;
        config.use_psram = true;

        for (size_t i = 0; i < channels.size() && i < 16; i++) {
            const auto &chipset = channels[i]->getChipset();
            const auto *spiConfig = chipset.ptr<SpiChipsetConfig>();
            if (spiConfig) {
                config.data_gpios[i] = spiConfig->dataPin;
                if (config.clock_gpio < 0) {
                    config.clock_gpio = spiConfig->clockPin;
                    config.clock_hz = spiConfig->timing.clock_hz;
                }
            }
        }

        if (config.dc_gpio < 0) {
            config.dc_gpio = 21;
        }

        if (!mPeripheral->initialize(config)) {
            FL_WARN("ChannelDriverLcdSpi: Failed to initialize peripheral");
            return false;
        }

        mNumLanes = numLanes;
        mInitialized = true;
    }

    // Allocate ring buffers (reused across frames)
    if (!allocateRingBuffers(slotCapacityBytes)) {
        return false;
    }

    // Register the ISR callback for chunked streaming
    if (!mPeripheral->registerTransmitCallback(
            reinterpret_cast<void *>(&isrChunkDone), this)) { // ok reinterpret cast
        FL_WARN("ChannelDriverLcdSpi: registerTransmitCallback failed");
        return false;
    }

    // Initialize ISR context for this frame
    mIsrCtx.reset();
    mIsrCtx.mTotalBytes = maxSize;
    mIsrCtx.mChunkInputBytes = chunkInputBytes;

    // Mark channels in-use (DMA sources must stay alive)
    for (const auto &channel : channels) {
        channel->setInUse(true);
    }

    // Transpose first chunk into ring[0]
    size_t firstChunkBytes = chunkInputBytes;
    if (firstChunkBytes > maxSize) {
        firstChunkBytes = maxSize;
    }

    fl::memset(mRingBuffers[0], 0, firstChunkBytes * 8 * sizeof(u16));
    transposeToWords(channels, mRingBuffers[0], 0, firstChunkBytes);

    mIsrCtx.mNextByteOffset = firstChunkBytes;
    mIsrCtx.mRingWriteIdx = 1; // next write goes to slot 1

    // Submit first chunk — ISR callback handles the rest
    mBusy = true;
    size_t firstDmaBytes = firstChunkBytes * 8 * sizeof(u16);
    if (!mPeripheral->transmit(mRingBuffers[0], firstDmaBytes)) {
        mBusy = false;
        for (const auto &channel : channels) {
            channel->setInUse(false);
        }
        FL_WARN("ChannelDriverLcdSpi: Initial transmit failed");
        return false;
    }

    return true;
}

//=============================================================================
// Factory
//=============================================================================

fl::shared_ptr<IChannelDriver> createLcdSpiEngine() FL_NOEXCEPT {
#if FL_LCD_SPI_HAS_ESP_PERIPHERAL
    class EspWrapper : public detail::ILcdSpiPeripheral {
      public:
        bool initialize(const detail::LcdSpiConfig &c) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().initialize(c);
        }
        void deinitialize() FL_NOEXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().deinitialize();
        }
        bool isInitialized() const FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().isInitialized();
        }
        u16 *allocateBuffer(size_t s) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().allocateBuffer(s);
        }
        void freeBuffer(u16 *b) FL_NOEXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().freeBuffer(b);
        }
        bool transmit(const u16 *b, size_t s) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().transmit(b, s);
        }
        bool waitTransmitDone(u32 t) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().waitTransmitDone(t);
        }
        bool isBusy() const FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().isBusy();
        }
        bool registerTransmitCallback(void *cb, void *ctx) FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance()
                .registerTransmitCallback(cb, ctx);
        }
        const detail::LcdSpiConfig &getConfig() const FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().getConfig();
        }
        u64 getMicroseconds() FL_NOEXCEPT override {
            return detail::LcdSpiPeripheralEsp::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NOEXCEPT override {
            detail::LcdSpiPeripheralEsp::instance().delay(ms);
        }
    };
    auto wrapper = fl::make_shared<EspWrapper>();
    return fl::make_shared<ChannelDriverLcdSpi>(wrapper);
#else
    return nullptr;
#endif
}

} // namespace fl

#undef FL_LCD_SPI_COMPILE

#endif // FL_LCD_SPI_COMPILE
