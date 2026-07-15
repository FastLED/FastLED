// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_uart.h"

#include "fl/stl/utility.h"

namespace fl {

namespace {
// RP2040 PL011 is clocked from clk_peri.  Keep a conservative 6.25 MHz
// ceiling here rather than inheriting ESP32's 5 MHz policy; the achieved-baud
// check below is the final hardware-specific timing admission.
constexpr u32 kRpUartMaxBaudRate = 6250000;
}

ChannelEngineRpUart::ChannelEngineRpUart(
    fl::shared_ptr<IRpUartPeripheral> peripheral, u8 uart_index) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)), mUartIndex(uart_index),
      mCurrentChannel(0), mLatchStartUs(0), mLatchDurationUs(0),
      mActive(false), mLatchPending(false), mFailed(false) {}

ChannelEngineRpUart::~ChannelEngineRpUart() {
    releaseInFlight();
    if (mPeripheral) {
        mPeripheral->abort();
        mPeripheral->deinitialize();
    }
}

bool ChannelEngineRpUart::isValidTxPin(int pin) const FL_NO_EXCEPT {
    static constexpr int kUart0TxPins[] = {0, 12, 16, 28};
    static constexpr int kUart1TxPins[] = {4, 8, 20, 24};
    const int* pins = mUartIndex == 0 ? kUart0TxPins : kUart1TxPins;
    for (size_t index = 0; index < 4; ++index) {
        if (pin == pins[index]) {
            return true;
        }
    }
    return false;
}

bool ChannelEngineRpUart::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    return data && data->isClockless() && isValidTxPin(data->getPin()) &&
           canRepresentTimingForMaxBaud(data->getTiming(), kRpUartMaxBaudRate);
}

void ChannelEngineRpUart::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData && canHandle(channelData)) {
        mPendingChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineRpUart::show() FL_NO_EXCEPT {
    if (mActive || mPendingChannels.empty()) {
        return;
    }
    mInFlightChannels = fl::move(mPendingChannels);
    mPendingChannels.clear();
    mCurrentChannel = 0;
    mFailed = false;
    mError.clear();
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) {
            channel->setInUse(true);
        }
    }
    mActive = true;
    if (!startNextTransmission()) {
        // show() has no status return. Preserve the error until the manager's
        // next poll, while still releasing the input buffers on that poll.
        mFailed = true;
        mError = "RP UART: unable to start queued channel";
    }
}

IChannelDriver::DriverState ChannelEngineRpUart::poll() FL_NO_EXCEPT {
    if (mFailed) {
        return fail(mError.c_str());
    }
    if (!mActive) {
        return DriverState::READY;
    }
    if (!mPeripheral) {
        return fail("RP UART: missing peripheral");
    }
    if (mPeripheral->hasError()) {
        return fail("RP UART: peripheral error");
    }
    if (mPeripheral->isDmaBusy()) {
        return DriverState::BUSY;
    }
    // PL011 TXFE may be set before the final byte leaves the shift register.
    // The peripheral checks UARTFR.BUSY, so DRAINING is an actual wire state.
    if (mPeripheral->isWireBusy()) {
        return DriverState::DRAINING;
    }
    if (!mLatchPending) {
        // A UART shift-register drain only establishes wire-idle. Keep the
        // line low for this chipset's reset/latch interval before either
        // reconfiguring the pin for another strip or releasing it to user
        // code. The peripheral-owned clock keeps host tests deterministic.
        mLatchDurationUs = mInFlightChannels[mCurrentChannel]->getTiming().reset_us;
        mLatchStartUs = mPeripheral->nowMicros();
        mLatchPending = true;
        if (mLatchDurationUs != 0) {
            return DriverState::DRAINING;
        }
    }
    if (static_cast<u32>(mPeripheral->nowMicros() - mLatchStartUs) <
        mLatchDurationUs) {
        return DriverState::DRAINING;
    }
    mLatchPending = false;
    ++mCurrentChannel;
    if (startNextTransmission()) {
        return DriverState::BUSY;
    }
    if (mCurrentChannel < mInFlightChannels.size()) {
        return fail("RP UART: unable to start queued channel");
    }
    mPeripheral->deinitialize();
    releaseInFlight();
    mActive = false;
    return DriverState::READY;
}

bool ChannelEngineRpUart::startNextTransmission() FL_NO_EXCEPT {
    while (mCurrentChannel < mInFlightChannels.size()) {
        if (beginTransmission(mInFlightChannels[mCurrentChannel])) {
            return true;
        }
        return false;
    }
    return false;
}

bool ChannelEngineRpUart::beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!mPeripheral || !canHandle(channel)) {
        return false;
    }
    const fl::vector_psram<u8>& input = channel->getData();
    if (input.empty()) {
        return false;
    }
    const Wave10Lut lut = buildWave10LutForMaxBaud(channel->getTiming(),
                                                    kRpUartMaxBaudRate);
    if (lut.pulses_per_bit == 0) {
        return false;
    }
    RpUartConfig config;
    config.uart_index = mUartIndex;
    config.tx_pin = static_cast<u8>(channel->getPin());
    config.baud_rate = lut.baudRate(channel->getTiming());
    config.data_bits = lut.dataBits();
    config.invert_tx = true;
    if (!mPeripheral->configure(config)) {
        return false;
    }
    const u32 actual_baud = mPeripheral->actualBaudRate();
    const u32 requested_baud = config.baud_rate;
    const u32 baud_error = actual_baud > requested_baud
                               ? actual_baud - requested_baud
                               : requested_baud - actual_baud;
    // The LUT admits half-pulse timing error. Limit the UART-divider error
    // to 1%, leaving that margin intact instead of silently stretching every
    // high/low symbol beyond the timing model.
    if (actual_baud == 0 || static_cast<u64>(baud_error) * 100u > requested_baud) {
        return false;
    }
    mEncodedBuffer.resize(calculateUartBufferSize(input.size()));
    const size_t encoded = encodeLedsToUart(input.data(), input.size(),
                                            mEncodedBuffer.data(),
                                            mEncodedBuffer.size(), lut);
    return encoded != 0 && mPeripheral->startTxDma(mEncodedBuffer.data(), encoded);
}

void ChannelEngineRpUart::releaseInFlight() FL_NO_EXCEPT {
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) {
            channel->setInUse(false);
        }
    }
    mInFlightChannels.clear();
    mEncodedBuffer.clear();
    mCurrentChannel = 0;
    mLatchStartUs = 0;
    mLatchDurationUs = 0;
    mLatchPending = false;
}

IChannelDriver::DriverState ChannelEngineRpUart::fail(const char* message) FL_NO_EXCEPT {
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
