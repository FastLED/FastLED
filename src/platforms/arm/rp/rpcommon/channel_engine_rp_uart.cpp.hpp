// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_uart.h"

#include "fl/stl/utility.h"

namespace fl {


ChannelEngineRpUart::ChannelEngineRpUart(
    fl::shared_ptr<IRpUartPeripheral> peripheral, u8 uart_index) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)), mUartIndex(uart_index),
      mCurrentChannel(0), mLatchStartUs(0), mLatchDurationUs(0),
      mActive(false), mLatchPending(false), mFailed(false),
      mLastStartAttempted(false), mLastStartSucceeded(false),
      mLastEncodedSize(0), mLastActualBaud(0) {}

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
    // Every call starts a fresh evaluation, so the previous run's numbers
    // stop being reportable here rather than in enqueue(). canHandle() is
    // also reached during driver selection without enqueue() following, which
    // is the path that left `attempted=True baud=2000000` visible on a
    // channel this engine had just refused. show() re-establishes these for a
    // channel that is accepted and actually runs.
    mLastStartAttempted = false;
    mLastStartSucceeded = false;
    mLastEncodedSize = 0;
    mLastActualBaud = 0;
    if (!data || !data->isClockless() || !isValidTxPin(data->getPin())) {
        // Say why. The three guards below all record a reason; this one did
        // not, so a decline for a pin this UART instance cannot drive was
        // indistinguishable from the engine never being consulted -- the
        // caller saw `attempted=false` with an empty error either way.
        // FastLED#4375.
        if (!data) {
            mLastError = "RP UART: no channel data";
        } else if (!data->isClockless()) {
            mLastError = "RP UART: channel is not clockless";
        } else {
            mLastError = "RP UART: TX pin is not usable by this UART instance";
        }
        return false;
    }
    // Ask the backend what it can actually reach rather than assuming a fixed
    // ceiling: the PL011 is bounded by clk_peri/16, so a slow clk_peri can put
    // the required baud out of range entirely. Record why we declined --
    // returning a bare false here left the diagnostic empty and made an
    // unreachable-baud board look identical to a wiring fault. See #3899.
    if (!mPeripheral) {
        mLastError = "RP UART: no peripheral backend";
        return false;
    }
    const u32 max_baud = mPeripheral->maxBaudRate();
    if (max_baud == 0) {
        mLastError = "RP UART: UART clock unavailable";
        return false;
    }
    if (!canRepresentTimingForMaxBaud(data->getTiming(), max_baud)) {
        // Deliberately does not name clk_peri: the ceiling is whatever the
        // backend reports, and only the RP hardware backend derives it from
        // clk_peri/16.
        mLastError = "RP UART: chipset timing needs a baud above the UART "
                     "backend maximum";
        return false;
    }
    // Clear the reason here rather than with the numerics above: show()
    // records a failed DMA start in mLastError and poll() is what reports it,
    // so an evaluation must not be able to wipe that. Accepting is the only
    // outcome that makes a previous decline's reason wrong, and this is the
    // only path that reaches it.
    mLastError.clear();
    return true;
}

void ChannelEngineRpUart::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    // canHandle() clears the numeric diagnostics and, on a decline, records
    // the reason -- so nothing is needed here. It is done there rather than
    // in show() because show() returns at its `mPendingChannels.empty()`
    // guard before reaching its own reset, and an empty queue is exactly what
    // a decline produces. Resetting above that guard would be worse: show()
    // runs every frame, so it would wipe a successful run's diagnostics
    // before the caller read them. See FastLED#4375.
    // No `channelData &&` short-circuit: that would skip canHandle() for a
    // null channel, leaving the diagnostics stale in exactly the case this
    // change exists to fix -- and making canHandle()'s own null branch
    // unreachable from here. canHandle() checks `!data` first, so calling it
    // unconditionally is safe and is what records the reason.
    if (canHandle(channelData)) {
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
    mLastStartAttempted = false;
    mLastStartSucceeded = false;
    mLastEncodedSize = 0;
    mLastActualBaud = 0;
    mLastError.clear();
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
        if (mError.empty()) {
            mError = "RP UART: unable to start queued channel";
            mLastError = mError;
        }
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
        return fail(mError.empty() ? "RP UART: unable to start queued channel"
                                   : mError.c_str());
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
        mError = "RP UART: invalid channel";
        mLastError = mError;
        return false;
    }
    const fl::vector_psram<u8>& input = channel->getData();
    if (input.empty()) {
        mError = "RP UART: empty channel";
        mLastError = mError;
        return false;
    }
    const Wave10Lut lut =
        buildWave10LutForMaxBaud(channel->getTiming(), mPeripheral->maxBaudRate());
    if (lut.pulses_per_bit == 0) {
        mError = "RP UART: timing is not representable";
        mLastError = mError;
        return false;
    }
    mLastStartAttempted = true;
    mLastStartSucceeded = false;
    mLastEncodedSize = 0;
    mLastActualBaud = 0;
    RpUartConfig config;
    config.uart_index = mUartIndex;
    config.tx_pin = static_cast<u8>(channel->getPin());
    config.baud_rate = lut.baudRate(channel->getTiming());
    config.data_bits = lut.dataBits();
    config.invert_tx = true;
    if (!mPeripheral->configure(config)) {
        mError = "RP UART: peripheral configure failed";
        mLastError = mError;
        return false;
    }
    const u32 actual_baud = mPeripheral->actualBaudRate();
    mLastActualBaud = actual_baud;
    const u32 requested_baud = config.baud_rate;
    const u32 baud_error = actual_baud > requested_baud
                               ? actual_baud - requested_baud
                               : requested_baud - actual_baud;
    // The LUT admits half-pulse timing error. Limit the UART-divider error
    // to 1%, leaving that margin intact instead of silently stretching every
    // high/low symbol beyond the timing model.
    if (actual_baud == 0 || static_cast<u64>(baud_error) * 100u > requested_baud) {
        mError = "RP UART: achieved baud outside tolerance";
        mLastError = mError;
        return false;
    }
    mEncodedBuffer.resize(calculateUartBufferSize(input.size()));
    const size_t encoded = encodeLedsToUart(input.data(), input.size(),
                                            mEncodedBuffer.data(),
                                            mEncodedBuffer.size(), lut);
    mLastEncodedSize = encoded;
    if (encoded == 0) {
        mError = "RP UART: encoding failed";
        mLastError = mError;
        return false;
    }
    if (!mPeripheral->startTxDma(mEncodedBuffer.data(), encoded)) {
        mError = "RP UART: DMA start failed";
        mLastError = mError;
        return false;
    }
    mLastStartSucceeded = true;
    mError.clear();
    mLastError.clear();
    return true;
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
    mLastError = message;
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
