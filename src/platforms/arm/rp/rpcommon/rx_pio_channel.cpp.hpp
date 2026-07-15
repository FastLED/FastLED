// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rx_pio_channel.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h"
#include "fl/channels/rx/decode_ws2812.h"
#include "fl/system/delay.h"
// IWYU pragma: begin_keep
#include "hardware/gpio.h"
#include "hardware/pio.h"
// IWYU pragma: end_keep

namespace fl {

fl::shared_ptr<RxDevice> RpPioRxDevice::create(int pin) FL_NO_EXCEPT {
    if (pin < 0 || pin >= RpResourceLedger::kMaxPins) return {};
    return fl::make_shared<RpPioRxDevice>(pin);
}

RpPioRxDevice::RpPioRxDevice(int pin) FL_NO_EXCEPT
    : mPin(pin), mPio(nullptr), mStateMachine(-1), mDmaChannel(-1), mCapacity(0),
      mArmed(false), mFinished(false), mOverflow(false) {}

RpPioRxDevice::~RpPioRxDevice() FL_NO_EXCEPT { stop(); }

bool RpPioRxDevice::begin(const RxConfig& config) FL_NO_EXCEPT {
    stop();
    if (config.buffer_size == 0) return false;
    auto& resources = RpPioDmaResourceManager::instance();
    PIO pio = nullptr;
    int state_machine = -1;
    int dma_channel = -1;
    if (!resources.claimPioStateMachine(&pio, &state_machine) ||
        !resources.claimDmaChannel(&dma_channel) ||
        !resources.claimPins(static_cast<u8>(mPin), 1)) {
        if (dma_channel >= 0) resources.releaseDmaChannel(dma_channel);
        if (state_machine >= 0) resources.releasePioStateMachine(pio, state_machine);
        return false;
    }
    gpio_init(static_cast<uint>(mPin));
    gpio_set_dir(static_cast<uint>(mPin), GPIO_IN);
    pio_gpio_init(pio, static_cast<uint>(mPin));
    pio_sm_set_consecutive_pindirs(pio, static_cast<uint>(state_machine),
                                   static_cast<uint>(mPin), 1, false);
    mPio = pio;
    mStateMachine = state_machine;
    mDmaChannel = dma_channel;
    mCapacity = config.buffer_size;
    mEdges.clear();
    mEdges.reserve(mCapacity);
    mArmed = true;
    mFinished = false;
    mOverflow = false;
    return true;
}

void RpPioRxDevice::stop() FL_NO_EXCEPT {
    if (mPio != nullptr && mStateMachine >= 0) {
        pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
        RpPioDmaResourceManager::instance().releasePioStateMachine(static_cast<PIO>(mPio), mStateMachine);
    }
    if (mDmaChannel >= 0) RpPioDmaResourceManager::instance().releaseDmaChannel(mDmaChannel);
    if (mArmed) RpPioDmaResourceManager::instance().releasePins(static_cast<u8>(mPin), 1);
    mPio = nullptr;
    mStateMachine = -1;
    mDmaChannel = -1;
    mArmed = false;
}

bool RpPioRxDevice::finished() const FL_NO_EXCEPT { return mFinished; }

RxWaitResult RpPioRxDevice::wait(u32 timeout_ms) FL_NO_EXCEPT {
    const u32 started = millis();
    while (!mFinished && millis() - started < timeout_ms) delay(1);
    if (!mFinished) return RxWaitResult::TIMEOUT;
    return mOverflow ? RxWaitResult::BUFFER_OVERFLOW : RxWaitResult::SUCCESS;
}

fl::result<u32, DecodeError> RpPioRxDevice::decode(const ChipsetTiming4Phase& timing,
                                                    fl::span<u8> out) FL_NO_EXCEPT {
    return fl::channels::rx::decodeWs2812Edges(timing, fl::span<const EdgeTime>(mEdges), out);
}

size_t RpPioRxDevice::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NO_EXCEPT {
    if (offset >= mEdges.size()) return 0;
    size_t count = mEdges.size() - offset;
    if (count > out.size()) count = out.size();
    for (size_t i = 0; i < count; ++i) out[i] = mEdges[offset + i];
    return count;
}

const char* RpPioRxDevice::name() const FL_NO_EXCEPT { return "PIO_RX"; }
int RpPioRxDevice::getPin() const FL_NO_EXCEPT { return mPin; }

bool RpPioRxDevice::injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT {
    if (!mArmed) return false;
    for (size_t i = 0; i < edges.size(); ++i) {
        if (mEdges.size() == mCapacity) { mOverflow = true; mFinished = true; return false; }
        mEdges.push_back(edges[i]);
    }
    mFinished = !edges.empty();
    return true;
}

}  // namespace fl

#endif
