// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.h"

#include "fl/stl/utility.h"

namespace fl {

namespace {

// Keep native PL022 SPI preferred whenever the selected pair has a legal
// hardware mux. FLEX_IO owns the remaining arbitrary PIO pin pairs.
bool isNativeSpiPinPair(const SpiChipsetConfig& config) FL_NO_EXCEPT {
    static constexpr u8 kMosi[] = {3, 7, 19, 23, 11, 15, 27};
    static constexpr u8 kSck[] = {2, 6, 18, 22, 10, 14, 26};
    for (size_t index = 0; index < sizeof(kMosi) / sizeof(kMosi[0]); ++index) {
        if (config.dataPin == kMosi[index] && config.clockPin == kSck[index]) {
            return true;
        }
    }
    return false;
}

}  // namespace

ChannelEngineRpPio::ChannelEngineRpPio(
    fl::shared_ptr<IRpPioTxPeripheral> peripheral,
    fl::shared_ptr<IRpPioSpiPeripheral> spi_peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)), mSpiPeripheral(fl::move(spi_peripheral)),
      mCurrentChannel(0), mLatchStartUs(0), mLatchDurationUs(0),
      mActiveLaneCount(1), mActiveMode(Mode::Clockless), mActive(false),
      mLatchPending(false), mFailed(false),
      mLastStartAttempted(false), mLastStartSucceeded(false), mLastWordCount(0) {}

ChannelEngineRpPio::~ChannelEngineRpPio() {
    releaseInFlight();
    if (mPeripheral) {
        mPeripheral->abort();
        mPeripheral->deinitialize();
    }
    if (mSpiPeripheral) {
        mSpiPeripheral->abort();
        mSpiPeripheral->deinitialize();
    }
}

bool ChannelEngineRpPio::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data) return false;
    if (data->isClockless()) {
        if (!mPeripheral || data->getPin() < 0 || data->getPin() > 29) return false;
        const ChipsetTimingConfig& timing = data->getTiming();
        // pio_gen needs at least one cycle in each segment and subtracts two from
        // the low tail instruction. Reject impossible runtime programs early.
        return timing.t1_ns != 0 && timing.t2_ns != 0 && timing.t3_ns != 0 &&
               timing.total_period_ns() >= 250;
    }
    const SpiChipsetConfig* spi = data->getChipset().ptr<SpiChipsetConfig>();
    return mSpiPeripheral && spi != nullptr && spi->dataPin >= 0 && spi->dataPin <= 29 &&
           spi->clockPin >= 0 && spi->clockPin <= 29 && spi->dataPin != spi->clockPin &&
           spi->timing.clock_hz != 0 && !isNativeSpiPinPair(*spi);
}

void ChannelEngineRpPio::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData && canHandle(channelData)) {
        mPendingChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineRpPio::show() FL_NO_EXCEPT {
    if (mActive || mPendingChannels.empty()) {
        return;
    }
    mInFlightChannels = fl::move(mPendingChannels);
    mPendingChannels.clear();
    mCurrentChannel = 0;
    mFailed = false;
    mLastStartAttempted = false;
    mLastStartSucceeded = false;
    mLastWordCount = 0;
    mError.clear();
    mLastError.clear();
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) channel->setInUse(true);
    }
    mActive = true;
    if (!startNextTransmission()) {
        mFailed = true;
        mError = "RP PIO: unable to start queued channel";
    }
}

IChannelDriver::DriverState ChannelEngineRpPio::poll() FL_NO_EXCEPT {
    if (mFailed) return fail(mError.c_str());
    if (!mActive) return DriverState::READY;
    if (mActiveMode == Mode::Spi) {
        if (!mSpiPeripheral) return fail("RP PIO SPI: missing peripheral");
        if (mSpiPeripheral->hasError()) return fail("RP PIO SPI: peripheral error");
        if (mSpiPeripheral->isDmaBusy()) return DriverState::BUSY;
        if (!mSpiPeripheral->isTerminalComplete()) return DriverState::DRAINING;
        ++mCurrentChannel;
        if (startNextTransmission()) return DriverState::BUSY;
        if (mCurrentChannel < mInFlightChannels.size()) {
            return fail("RP PIO SPI: unable to start queued channel");
        }
        mSpiPeripheral->deinitialize();
        releaseInFlight();
        mActive = false;
        return DriverState::READY;
    }
    if (!mPeripheral) return fail("RP PIO: missing peripheral");
    if (mPeripheral->hasError()) return fail("RP PIO: peripheral error");
    if (mPeripheral->isDmaBusy()) return DriverState::BUSY;
    if (!mPeripheral->isTerminalComplete()) return DriverState::DRAINING;

    if (!mLatchPending) {
        mLatchDurationUs = mInFlightChannels[mCurrentChannel]->getTiming().reset_us;
        mLatchStartUs = mPeripheral->nowMicros();
        mLatchPending = true;
        if (mLatchDurationUs != 0) return DriverState::DRAINING;
    }
    if (static_cast<u32>(mPeripheral->nowMicros() - mLatchStartUs) < mLatchDurationUs) {
        return DriverState::DRAINING;
    }
    mLatchPending = false;
    mCurrentChannel += mActiveLaneCount;
    if (startNextTransmission()) return DriverState::BUSY;
    if (mCurrentChannel < mInFlightChannels.size()) {
        return fail("RP PIO: unable to start queued channel");
    }
    mPeripheral->deinitialize();
    releaseInFlight();
    mActive = false;
    return DriverState::READY;
}

bool ChannelEngineRpPio::startNextTransmission() FL_NO_EXCEPT {
    return mCurrentChannel < mInFlightChannels.size() &&
           beginTransmission(mInFlightChannels[mCurrentChannel]);
}

bool ChannelEngineRpPio::beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!canHandle(channel)) return false;
    if (channel->isSpi()) {
        const fl::vector_psram<u8>& input = channel->getData();
        const SpiChipsetConfig& chipset = channel->getChipset().get<SpiChipsetConfig>();
        if (input.empty() || !mSpiPeripheral) return false;
        RpPioSpiConfig config;
        config.mosi_pin = static_cast<u8>(chipset.dataPin);
        config.sck_pin = static_cast<u8>(chipset.clockPin);
        config.clock_hz = chipset.timing.clock_hz;
        if (!mSpiPeripheral->configure(config)) return false;
        mPioWords.clear();
        mPioWords.reserve(input.size());
        for (u8 byte : input) {
            mPioWords.push_back(static_cast<u32>(byte) << 24);
        }
        mActiveLaneCount = 1;
        mActiveMode = Mode::Spi;
        mLastStartAttempted = true;
        mLastWordCount = mPioWords.size();
        mLastStartSucceeded = mSpiPeripheral->startTxDma(mPioWords.data(), mPioWords.size());
        return mLastStartSucceeded;
    }
    if (!mPeripheral) return false;
    const fl::vector_psram<u8>& input = channel->getData();
    if (input.empty()) return false;
    // Batch only an adjacent run that is provably safe: same wire timing,
    // byte count, and consecutive pins. This keeps a short/mismatched strip
    // from receiving padding or a different timing waveform.
    u8 run = 1;
    while (run < 8 && mCurrentChannel + run < mInFlightChannels.size()) {
        const ChannelDataPtr& next = mInFlightChannels[mCurrentChannel + run];
        if (!next || next->getPin() != channel->getPin() + run ||
            next->getTiming() != channel->getTiming() ||
            next->getData().size() != input.size()) break;
        ++run;
    }
    mActiveLaneCount = run >= 8 ? 8 : run >= 4 ? 4 : run >= 2 ? 2 : 1;
    mActiveMode = Mode::Clockless;
    RpPioTxConfig config;
    config.tx_pin = static_cast<u8>(channel->getPin());
    config.lane_count = mActiveLaneCount;
    config.timing = channel->getTiming();
    if (!mPeripheral->configure(config)) return false;

    // Autopull is configured for eight bits. Every byte occupies the MSB of
    // one DMA word, so the PIO emits exactly the requested bytes: no final
    // zero-padding can become a partial extra LED symbol.
    mPioWords.clear();
    for (size_t byte_index = 0; byte_index < input.size(); ++byte_index) {
        for (int bit = 7; bit >= 0; --bit) {
            u32 plane = 0;
            for (u8 lane = 0; lane < mActiveLaneCount; ++lane) {
                const u8 value = mInFlightChannels[mCurrentChannel + lane]
                                     ->getData()[byte_index];
                plane |= static_cast<u32>((value >> bit) & 1u)
                         << (mActiveLaneCount - 1u - lane);
            }
            mPioWords.push_back(plane << (32u - mActiveLaneCount));
        }
    }
    mLastStartAttempted = true;
    mLastWordCount = mPioWords.size();
    mLastStartSucceeded = mPeripheral->startTxDma(mPioWords.data(), mPioWords.size());
    return mLastStartSucceeded;
}

void ChannelEngineRpPio::releaseInFlight() FL_NO_EXCEPT {
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) channel->setInUse(false);
    }
    mInFlightChannels.clear();
    mPioWords.clear();
    mCurrentChannel = 0;
    mLatchStartUs = 0;
    mLatchDurationUs = 0;
    mLatchPending = false;
    mActiveLaneCount = 1;
}

IChannelDriver::DriverState ChannelEngineRpPio::fail(const char* message) FL_NO_EXCEPT {
    mLastError = message;
    if (mPeripheral) {
        mPeripheral->abort();
        mPeripheral->deinitialize();
    }
    if (mSpiPeripheral) {
        mSpiPeripheral->abort();
        mSpiPeripheral->deinitialize();
    }
    releaseInFlight();
    mActive = false;
    mFailed = false;
    return DriverState(DriverState::ERROR, fl::string(message));
}

}  // namespace fl
