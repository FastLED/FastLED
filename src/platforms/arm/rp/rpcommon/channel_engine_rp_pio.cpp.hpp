// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.h"

#include "fl/stl/utility.h"

namespace fl {

ChannelEngineRpPio::ChannelEngineRpPio(
    fl::shared_ptr<IRpPioTxPeripheral> peripheral, u8 which) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)), mWhich(which), mCurrentChannel(0),
      mLatchStartUs(0), mLatchDurationUs(0), mActive(false),
      mLatchPending(false), mFailed(false) {}

ChannelEngineRpPio::~ChannelEngineRpPio() {
    releaseInFlight();
    if (mPeripheral) {
        mPeripheral->abort();
        mPeripheral->deinitialize();
    }
}

bool ChannelEngineRpPio::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !data->isClockless() || data->getPin() < 0 || data->getPin() > 29) {
        return false;
    }
    const ChipsetTimingConfig& timing = data->getTiming();
    // pio_gen needs at least one cycle in each segment and subtracts two from
    // the low tail instruction. Reject impossible runtime programs early.
    return timing.t1_ns != 0 && timing.t2_ns != 0 && timing.t3_ns != 0 &&
           timing.total_period_ns() >= 250;
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
    mError.clear();
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
    ++mCurrentChannel;
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
    if (!mPeripheral || !canHandle(channel)) return false;
    const fl::vector_psram<u8>& input = channel->getData();
    if (input.empty()) return false;
    RpPioTxConfig config;
    config.tx_pin = static_cast<u8>(channel->getPin());
    config.timing = channel->getTiming();
    if (!mPeripheral->configure(config)) return false;

    // Autopull is configured for eight bits. Every byte occupies the MSB of
    // one DMA word, so the PIO emits exactly the requested bytes: no final
    // zero-padding can become a partial extra LED symbol.
    mPioWords.resize(input.size());
    for (size_t index = 0; index < input.size(); ++index) {
        mPioWords[index] = static_cast<u32>(input[index]) << 24;
    }
    return mPeripheral->startTxDma(mPioWords.data(), mPioWords.size());
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
}

IChannelDriver::DriverState ChannelEngineRpPio::fail(const char* message) FL_NO_EXCEPT {
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
