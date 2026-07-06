// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_UART_DMA)

#include "platforms/arm/lpc/drivers/uart_dma/channel_engine_lpc_uart_dma.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/log/log.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/utility.h"

namespace fl {

namespace {

struct LpcUartWave10Lut {
    u8 lut[4] = {0, 0, 0, 0};
    u32 baud = 0;
    bool ok = false;
};

u32 lpcUartAbsDiff(u32 a, u32 b) FL_NO_EXCEPT {
    return a > b ? a - b : b - a;
}

u8 lpcUartComputePulseCount(u32 timingNs, u32 pulseWidthNs) FL_NO_EXCEPT {
    if (pulseWidthNs == 0) {
        return 0;
    }
    const u8 lo = static_cast<u8>(timingNs / pulseWidthNs);
    const u8 hi = static_cast<u8>(lo + 1u);
    const u32 errLo = lpcUartAbsDiff(static_cast<u32>(lo) * pulseWidthNs, timingNs);
    const u32 errHi = lpcUartAbsDiff(static_cast<u32>(hi) * pulseWidthNs, timingNs);
    return errHi < errLo ? hi : lo;
}

u8 buildUartByte(u8 pulsesA, u8 pulsesB) FL_NO_EXCEPT {
    int wire[10];
    for (int i = 0; i < 5; ++i) {
        wire[i] = i < static_cast<int>(pulsesA) ? 1 : 0;
        wire[5 + i] = i < static_cast<int>(pulsesB) ? 1 : 0;
    }

    u8 value = 0;
    for (int i = 0; i < 8; ++i) {
        const int dataBit = wire[1 + i] ? 0 : 1;
        value |= static_cast<u8>(dataBit << i);
    }
    return value;
}

LpcUartWave10Lut buildLpcUartWave10Lut(
        const ChipsetTimingConfig& timing) FL_NO_EXCEPT {
    LpcUartWave10Lut result;
    constexpr u8 kPulsesPerBit = 5;
    constexpr u32 kMaxBaud = 4000000;
    constexpr u32 kMinSymbolSeparationNs = 400;

    const u32 periodNs = timing.total_period_ns();
    if (periodNs == 0) {
        return result;
    }
    const u32 baud = static_cast<u32>(
        static_cast<u64>(kPulsesPerBit) * 1000000000ULL / periodNs);
    if (baud == 0 || baud > kMaxBaud) {
        return result;
    }

    const u32 pulseWidthNs = periodNs / kPulsesPerBit;
    const u32 t0hNs = timing.t1_ns;
    const u32 t1hNs = timing.t1_ns + timing.t2_ns;
    u8 pulses0 = lpcUartComputePulseCount(t0hNs, pulseWidthNs);
    u8 pulses1 = lpcUartComputePulseCount(t1hNs, pulseWidthNs);
    if (pulses0 < 1 || pulses0 > 4 || pulses1 < 1 || pulses1 > 4) {
        return result;
    }
    if (pulses0 == pulses1) {
        return result;
    }
    if (static_cast<u32>(pulses1 - pulses0) * pulseWidthNs <
        kMinSymbolSeparationNs) {
        return result;
    }

    const u32 toleranceNs = pulseWidthNs / 2;
    const u32 err0 = lpcUartAbsDiff(static_cast<u32>(pulses0) * pulseWidthNs, t0hNs);
    const u32 err1 = lpcUartAbsDiff(static_cast<u32>(pulses1) * pulseWidthNs, t1hNs);
    if (err0 > toleranceNs || err1 > toleranceNs) {
        return result;
    }

    result.lut[0] = buildUartByte(pulses0, pulses0);
    result.lut[1] = buildUartByte(pulses0, pulses1);
    result.lut[2] = buildUartByte(pulses1, pulses0);
    result.lut[3] = buildUartByte(pulses1, pulses1);
    result.baud = baud;
    result.ok = true;
    return result;
}

bool lpcUartCanRepresentTiming(
        const ChipsetTimingConfig& timing) FL_NO_EXCEPT {
    return buildLpcUartWave10Lut(timing).ok;
}

size_t encodeLpcUartBytes(const u8* input, size_t inputSize, u8* output,
                          size_t outputCapacity,
                          const LpcUartWave10Lut& lut) FL_NO_EXCEPT {
    const size_t required = inputSize * 4u;
    if (required > outputCapacity) {
        return 0;
    }
    for (size_t i = 0; i < inputSize; ++i) {
        const u8 byte = input[i];
        output[i * 4u + 0u] = lut.lut[(byte >> 6) & 0x03u];
        output[i * 4u + 1u] = lut.lut[(byte >> 4) & 0x03u];
        output[i * 4u + 2u] = lut.lut[(byte >> 2) & 0x03u];
        output[i * 4u + 3u] = lut.lut[byte & 0x03u];
    }
    return required;
}

}  // namespace

bool ChannelEngineLpcUartDma::canHandle(
        const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !data->isClockless()) {
        return false;
    }
    const int pin = data->getPin();
    if (pin < 0 || pin > 31) {
        return false;
    }
    return lpcUartCanRepresentTiming(data->getTiming());
}

void ChannelEngineLpcUartDma::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEngineLpcUartDma::show() FL_NO_EXCEPT {
    if (mTransmissionActive || mEnqueuedChannels.empty()) {
        return;
    }
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();
    mCurrentChannel = 0;
    startNextTransmission();
}

IChannelDriver::DriverState ChannelEngineLpcUartDma::poll() FL_NO_EXCEPT {
    if (!mTransmissionActive) {
        return DriverState::READY;
    }
    if (!lpc::LpcUartDmaRuntime::streamDone()) {
        return DriverState::DRAINING;
    }
    if (mCurrentChannel < mTransmittingChannels.size() &&
        mTransmittingChannels[mCurrentChannel]) {
        mTransmittingChannels[mCurrentChannel]->setInUse(false);
    }
    ++mCurrentChannel;
    if (startNextTransmission()) {
        return DriverState::DRAINING;
    }

    mTransmissionActive = false;
    mTransmittingChannels.clear();
    mCurrentChannel = 0;
    mPollNeededCallback.invoke();
    return DriverState::READY;
}

bool ChannelEngineLpcUartDma::startNextTransmission() FL_NO_EXCEPT {
    while (mCurrentChannel < mTransmittingChannels.size()) {
        if (beginTransmission(mTransmittingChannels[mCurrentChannel])) {
            mTransmissionActive = true;
            return true;
        }
        if (mTransmittingChannels[mCurrentChannel]) {
            mTransmittingChannels[mCurrentChannel]->setInUse(false);
        }
        ++mCurrentChannel;
    }
    return false;
}

bool ChannelEngineLpcUartDma::beginTransmission(
        const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!channel || !canHandle(channel)) {
        return false;
    }
    const fl::vector_psram<u8>& data = channel->getData();
    if (data.empty()) {
        return false;
    }

    const LpcUartWave10Lut lut = buildLpcUartWave10Lut(channel->getTiming());
    if (!lut.ok) {
        return false;
    }

    const size_t encodedSize = data.size() * 4u;
    mEncodedBuffer.resize(encodedSize);
    const size_t written = encodeLpcUartBytes(
        data.data(), data.size(), mEncodedBuffer.data(), mEncodedBuffer.size(),
        lut);
    if (written == 0) {
        FL_WARN_F("LPC UART DMA: encode failed for %u bytes",
                  static_cast<unsigned>(data.size()));
        return false;
    }

    lpc::LpcUartDmaRuntime::init(
        static_cast<u8>(channel->getPin()), lut.baud, /*invertTx=*/true);
    if (!lpc::LpcUartDmaRuntime::kickDmaStreamAsync(
            mEncodedBuffer.data(), static_cast<u32>(written))) {
        FL_WARN_F("LPC UART DMA: DMA stream kick failed");
        return false;
    }
    channel->setInUse(true);
    return true;
}

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA
