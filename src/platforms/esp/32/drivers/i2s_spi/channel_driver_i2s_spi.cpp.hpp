// IWYU pragma: private

/// @file channel_driver_i2s_spi.cpp
/// @brief I2S parallel SPI channel driver implementation

#include "platforms/is_platform.h"

// Compile for ESP32dev (with I2S parallel) or host testing
#if defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) &&          \
     (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))
#define FL_I2S_SPI_COMPILE 1
#elif defined(FL_IS_ESP32)
#include "sdkconfig.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_I2S
#define FL_I2S_SPI_COMPILE 1
#endif
#endif

#ifdef FL_I2S_SPI_COMPILE

#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.h"
#include "fl/log/log.h"
#include "fl/task/executor.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

// Include ESP peripheral for factory function
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S
// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_esp.h"
// IWYU pragma: end_keep
#endif

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelDriverI2sSpi::ChannelDriverI2sSpi(
    fl::shared_ptr<detail::II2sSpiPeripheral> peripheral) FL_NOEXCEPT
    : mPeripheral(fl::move(peripheral)), mInitialized(false),
      mEnqueuedChannels(), mTransmittingChannels(), mBuffer(nullptr),
      mBufferSize(0), mBusy(false) {}

ChannelDriverI2sSpi::~ChannelDriverI2sSpi() {
    // Block until DMA completes before freeing resources.
    if (mBusy && mPeripheral) {
        mPeripheral->waitTransmitDone(2000);
        mBusy = false;
    }
    if (mPeripheral && mBuffer != nullptr) {
        mPeripheral->freeBuffer(mBuffer);
        mBuffer = nullptr;
    }
}

//=============================================================================
// IChannelDriver Interface
//=============================================================================

bool ChannelDriverI2sSpi::canHandle(
    const ChannelDataPtr &data) const FL_NOEXCEPT {
    if (!data) {
        return false;
    }
    return data->isSpi(); // Only handle true SPI chipsets
}

void ChannelDriverI2sSpi::enqueue(ChannelDataPtr channelData) FL_NOEXCEPT {
    mEnqueuedChannels.push_back(fl::move(channelData));
}

void ChannelDriverI2sSpi::show() FL_NOEXCEPT {
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
        FL_WARN("ChannelDriverI2sSpi: DMA hung — forcing release");
        mBusy = false;
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }

    // Move enqueued to transmitting
    mTransmittingChannels = fl::move(mEnqueuedChannels);

    // Begin transmission (blocking)
    if (!beginTransmission(mTransmittingChannels)) {
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }
}

IChannelDriver::DriverState ChannelDriverI2sSpi::poll() FL_NOEXCEPT {
    if (mTransmittingChannels.empty()) {
        return DriverState::READY;
    }

    if (mPeripheral && !mPeripheral->isBusy()) {
        mBusy = false;
        for (auto &channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
        return DriverState::READY;
    }

    return mBusy ? DriverState::DRAINING : DriverState::READY;
}

//=============================================================================
// Transmission
//=============================================================================

bool ChannelDriverI2sSpi::beginTransmission(
    fl::span<const ChannelDataPtr> channels) FL_NOEXCEPT {
    if (channels.empty() || !mPeripheral) {
        return false;
    }

    // Find max channel data size
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

    // Calculate interleaved buffer size:
    // For N lanes, each byte position has N bytes (one per lane)
    size_t interleavedSize = maxSize * static_cast<size_t>(numLanes);

    // Initialize peripheral if needed
    if (!mInitialized || mBufferSize < interleavedSize) {
        // Free old buffer
        if (mBuffer != nullptr) {
            mPeripheral->freeBuffer(mBuffer);
            mBuffer = nullptr;
        }

        if (mInitialized) {
            mPeripheral->deinitialize();
            mInitialized = false;
        }

        // Configure peripheral from channel data
        detail::I2sSpiConfig config;
        config.num_lanes = numLanes;
        config.max_transfer_bytes = interleavedSize;

        // Get clock pin and data pins from SPI chipset config
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

        if (!mPeripheral->initialize(config)) {
            FL_WARN("ChannelDriverI2sSpi: Failed to initialize peripheral");
            return false;
        }

        // Allocate buffer
        mBuffer = mPeripheral->allocateBuffer(interleavedSize);
        if (mBuffer == nullptr) {
            FL_WARN("ChannelDriverI2sSpi: Failed to allocate buffer");
            return false;
        }
        mBufferSize = interleavedSize;
        mInitialized = true;
    }

    // Interleave channel data into buffer
    // Format: [lane0_byte0, lane1_byte0, ..., lane0_byte1, lane1_byte1, ...]
    fl::memset(mBuffer, 0, interleavedSize);

    for (size_t lane = 0; lane < channels.size(); lane++) {
        const auto &data = channels[lane]->getData();
        for (size_t byteIdx = 0; byteIdx < data.size(); byteIdx++) {
            mBuffer[byteIdx * numLanes + lane] = data[byteIdx];
        }
    }

    // Mark channels in use
    for (const auto &channel : channels) {
        channel->setInUse(true);
    }

    // Transmit (blocking for now)
    mBusy = true;
    if (!mPeripheral->transmit(mBuffer, interleavedSize)) {
        mBusy = false;
        for (const auto &channel : channels) {
            channel->setInUse(false);
        }
        FL_WARN("ChannelDriverI2sSpi: Transmit failed");
        return false;
    }

    // show() returns immediately — poll() drives completion via DRAINING state.
    return true;
}

//=============================================================================
// Factory Function
//=============================================================================

fl::shared_ptr<IChannelDriver> createI2sSpiEngine() FL_NOEXCEPT {
#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S
    // Wrap singleton ESP peripheral in shared_ptr via delegating wrapper
    class EspWrapper : public detail::II2sSpiPeripheral {
      public:
        bool initialize(const detail::I2sSpiConfig &c) FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().initialize(c);
        }
        void deinitialize() FL_NOEXCEPT override {
            detail::I2sSpiPeripheralEsp::instance().deinitialize();
        }
        bool isInitialized() const FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().isInitialized();
        }
        u8 *allocateBuffer(size_t s) FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().allocateBuffer(s);
        }
        void freeBuffer(u8 *b) FL_NOEXCEPT override {
            detail::I2sSpiPeripheralEsp::instance().freeBuffer(b);
        }
        bool transmit(const u8 *b, size_t s) FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().transmit(b, s);
        }
        bool waitTransmitDone(u32 t) FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().waitTransmitDone(t);
        }
        bool isBusy() const FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().isBusy();
        }
        bool registerTransmitCallback(void *cb, void *ctx) FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance()
                .registerTransmitCallback(cb, ctx);
        }
        const detail::I2sSpiConfig &getConfig() const FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().getConfig();
        }
        u64 getMicroseconds() FL_NOEXCEPT override {
            return detail::I2sSpiPeripheralEsp::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NOEXCEPT override {
            detail::I2sSpiPeripheralEsp::instance().delay(ms);
        }
    };
    auto wrapper = fl::make_shared<EspWrapper>();
    return fl::make_shared<ChannelDriverI2sSpi>(wrapper);
#else
    return nullptr;
#endif
}

} // namespace fl

#undef FL_I2S_SPI_COMPILE

#endif // FL_I2S_SPI_COMPILE
