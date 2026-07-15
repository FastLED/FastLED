// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_spi.h"

#include "fl/stl/utility.h"

namespace fl {

namespace {
constexpr u32 kRpSpiMaxClockHz = 62500000;
constexpr u32 kRpSpiTimeoutUs = 1000000;
}

ChannelEngineRpSpi::ChannelEngineRpSpi(
    fl::shared_ptr<IRpSpiPeripheral> peripheral, u8 spi_index) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)), mSpiIndex(spi_index),
      mCurrentChannel(0), mStartUs(0), mLastActualClockHz(0),
      mRxCapture(nullptr), mRxCaptureSize(0), mActive(false), mFailed(false) {}

ChannelEngineRpSpi::~ChannelEngineRpSpi() {
    releaseInFlight();
    if (mPeripheral) {
        mPeripheral->abort();
        mPeripheral->deinitialize();
    }
}

bool ChannelEngineRpSpi::pinsForChannel(const ChannelDataPtr& channel,
                                        u8* miso_pin) const FL_NO_EXCEPT {
    if (!channel || !channel->isSpi() || miso_pin == nullptr) return false;
    const SpiChipsetConfig* config = channel->getChipset().ptr<SpiChipsetConfig>();
    if (config == nullptr) return false;
    static constexpr u8 kSpi0Mosi[] = {3, 7, 19, 23};
    static constexpr u8 kSpi0Sck[] = {2, 6, 18, 22};
    static constexpr u8 kSpi0Miso[] = {0, 4, 16, 20};
    static constexpr u8 kSpi1Mosi[] = {11, 15, 27};
    static constexpr u8 kSpi1Sck[] = {10, 14, 26};
    static constexpr u8 kSpi1Miso[] = {8, 12, 24};
    const u8* mosi = mSpiIndex == 0 ? kSpi0Mosi : kSpi1Mosi;
    const u8* sck = mSpiIndex == 0 ? kSpi0Sck : kSpi1Sck;
    const u8* miso = mSpiIndex == 0 ? kSpi0Miso : kSpi1Miso;
    const size_t count = mSpiIndex == 0 ? 4 : (mSpiIndex == 1 ? 3 : 0);
    for (size_t index = 0; index < count; ++index) {
        if (config->dataPin == mosi[index] && config->clockPin == sck[index]) {
            *miso_pin = miso[index];
            return true;
        }
    }
    return false;
}

bool ChannelEngineRpSpi::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    u8 miso_pin = 0;
    const SpiChipsetConfig* config = data ? data->getChipset().ptr<SpiChipsetConfig>() : nullptr;
    return config != nullptr && config->timing.clock_hz != 0 &&
           pinsForChannel(data, &miso_pin);
}

void ChannelEngineRpSpi::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData && canHandle(channelData)) mPendingChannels.push_back(fl::move(channelData));
}

void ChannelEngineRpSpi::show() FL_NO_EXCEPT {
    if (mActive || mPendingChannels.empty()) return;
    mInFlightChannels = fl::move(mPendingChannels);
    mPendingChannels.clear();
    mCurrentChannel = 0;
    mFailed = false;
    mError.clear();
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) channel->setInUse(true);
    }
    mActive = true;
    if (!startNextTransmission()) {
        mFailed = true;
        mError = "RP SPI: unable to start queued channel";
    }
}

bool ChannelEngineRpSpi::captureNextRxBytes(u8* data, size_t size) FL_NO_EXCEPT {
    if (mActive || data == nullptr || size == 0) return false;
    mRxCapture = data;
    mRxCaptureSize = size;
    return true;
}

IChannelDriver::DriverState ChannelEngineRpSpi::poll() FL_NO_EXCEPT {
    if (mFailed) return fail(mError.c_str());
    if (!mActive) return DriverState::READY;
    if (!mPeripheral) return fail("RP SPI: missing peripheral");
    if (mPeripheral->hasError()) return fail("RP SPI: peripheral error");
    if (static_cast<u32>(mPeripheral->nowMicros() - mStartUs) > kRpSpiTimeoutUs) {
        return fail("RP SPI: transfer timeout");
    }
    if (mPeripheral->isTxDmaBusy() || mPeripheral->isRxDmaBusy()) {
        return DriverState::BUSY;
    }
    // TX DMA completing only proves the FIFO accepted the bytes. PL022 BUSY
    // is the wire-idle proof and must clear before the channel is released.
    if (mPeripheral->isWireBusy()) return DriverState::DRAINING;
    ++mCurrentChannel;
    if (startNextTransmission()) return DriverState::BUSY;
    if (mCurrentChannel < mInFlightChannels.size()) {
        return fail("RP SPI: unable to start queued channel");
    }
    mPeripheral->deinitialize();
    releaseInFlight();
    mActive = false;
    return DriverState::READY;
}

bool ChannelEngineRpSpi::startNextTransmission() FL_NO_EXCEPT {
    return mCurrentChannel < mInFlightChannels.size() &&
           beginTransmission(mInFlightChannels[mCurrentChannel]);
}

bool ChannelEngineRpSpi::beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!mPeripheral || !canHandle(channel) || channel->getData().empty()) return false;
    const SpiChipsetConfig& chipset = channel->getChipset().get<SpiChipsetConfig>();
    u8 miso_pin = 0;
    if (!pinsForChannel(channel, &miso_pin)) return false;
    RpSpiConfig config;
    config.spi_index = mSpiIndex;
    config.mosi_pin = static_cast<u8>(chipset.dataPin);
    config.miso_pin = miso_pin;
    config.sck_pin = static_cast<u8>(chipset.clockPin);
    config.clock_hz = chipset.timing.clock_hz > kRpSpiMaxClockHz
                          ? kRpSpiMaxClockHz : chipset.timing.clock_hz;
    // All supported clocked LED encoders emit eight-bit, MSB-first mode-0.
    config.data_bits = 8;
    config.cpol = false;
    config.cpha = false;
    config.msb_first = true;
    if (!mPeripheral->configure(config) || mPeripheral->actualClockHz() == 0) {
        return false;
    }
    mLastActualClockHz = mPeripheral->actualClockHz();
    const bool capture_requested = mRxCapture != nullptr;
    const bool started = capture_requested
        ? mPeripheral->startTxDmaCaptureRx(channel->getData().data(),
                                           channel->getData().size(), mRxCapture,
                                           mRxCaptureSize)
        : mPeripheral->startTxDma(channel->getData().data(), channel->getData().size());
    mRxCapture = nullptr;
    mRxCaptureSize = 0;
    if (!started) return false;
    mStartUs = mPeripheral->nowMicros();
    return true;
}

void ChannelEngineRpSpi::releaseInFlight() FL_NO_EXCEPT {
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) channel->setInUse(false);
    }
    mInFlightChannels.clear();
    mCurrentChannel = 0;
    mStartUs = 0;
    mRxCapture = nullptr;
    mRxCaptureSize = 0;
}

IChannelDriver::DriverState ChannelEngineRpSpi::fail(const char* message) FL_NO_EXCEPT {
    if (mPeripheral) {
        mPeripheral->abort();
        mPeripheral->deinitialize();
    }
    releaseInFlight();
    mActive = false;
    mFailed = false;
    return DriverState(DriverState::ERROR, fl::string(message));
}

}  // namespace fl
