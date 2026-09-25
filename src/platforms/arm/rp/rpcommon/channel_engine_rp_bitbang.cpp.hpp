// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_bitbang.h"

#include "fl/stl/move.h"
#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
// IWYU pragma: begin_keep
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/time.h"
// IWYU pragma: end_keep
#endif

namespace fl {

ChannelEngineRpBitBang::ChannelEngineRpBitBang(fl::shared_ptr<IRpBitBangPin> sink,
                                               u32 cpuHz,
                                               const char* driver_name) FL_NO_EXCEPT
    : mSink(fl::move(sink)), mCpuHz(cpuHz), mDriverName(driver_name) {}

bool ChannelEngineRpBitBang::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !mSink || !data->isClockless()) return false;
    if (data->getPin() < 0 || data->getPin() > 29) return false;
    const ChipsetTimingConfig& timing = data->getTiming();
    return timing.t1_ns != 0 && timing.t3_ns != 0;
}

void ChannelEngineRpBitBang::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData && canHandle(channelData)) {
        mPendingChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineRpBitBang::show() FL_NO_EXCEPT {
    if (mPendingChannels.empty()) return;
    fl::vector<ChannelDataPtr> channels = fl::move(mPendingChannels);
    mPendingChannels.clear();
    for (const ChannelDataPtr& channel : channels) {
        channel->setInUse(true);
        const ChipsetTimingConfig& timing = channel->getTiming();
        // Convert once per channel, never per bit.
        const RpBitBangBitCycles cycles = rpBitBangBitCycles(timing, mCpuHz);
        const fl::vector_psram<u8>& bytes = channel->getData();
        mSink->begin(channel->getPin());
        rpBitBangEmitBytes(*mSink, cycles, bytes.data(), bytes.size(),
                           channel->getExtraZeroBitsPerByte());
        mSink->end();
        mSink->latch(timing.reset_us);
        channel->setInUse(false);
    }
}

IChannelDriver::DriverState ChannelEngineRpBitBang::poll() FL_NO_EXCEPT {
    // show() is blocking and already honored the reset latch.
    return DriverState::READY;
}

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

namespace {

/// Approximate cycles spent per set() call outside the delay loop
/// (virtual dispatch, SIO write, loop setup). Subtracted from each hold.
constexpr u32 kRpBitBangSetOverheadCycles = 12;

/// Busy-wait roughly `cycles` CPU cycles: subs (1) + taken bne (2) per turn.
static inline __attribute__((always_inline)) void rpBitBangDelay(u32 cycles) FL_NO_EXCEPT {
    u32 loops = cycles / 3u;
    if (loops == 0) return;
    __asm__ volatile("1: subs %0, %0, #1\n\tbne 1b\n" : "+l"(loops) : : "cc");
}

class RpBitBangDevicePin final : public IRpBitBangPin {
  public:
    void begin(int pin) FL_NO_EXCEPT override {
        mMask = 1u << static_cast<u32>(pin);
        gpio_init(static_cast<uint>(pin));
        gpio_put(static_cast<uint>(pin), false);
        gpio_set_dir(static_cast<uint>(pin), GPIO_OUT);
        mIrqState = save_and_disable_interrupts();
    }
    void __not_in_flash_func(set)(bool high, u32 holdCycles) FL_NO_EXCEPT override {
        if (high) {
            sio_hw->gpio_set = mMask;
        } else {
            sio_hw->gpio_clr = mMask;
        }
        rpBitBangDelay(holdCycles > kRpBitBangSetOverheadCycles
                           ? holdCycles - kRpBitBangSetOverheadCycles
                           : 0);
    }
    void end() FL_NO_EXCEPT override {
        sio_hw->gpio_clr = mMask;
        restore_interrupts(mIrqState);
    }
    void latch(u32 resetUs) FL_NO_EXCEPT override {
        if (resetUs != 0) busy_wait_us_32(resetUs);
    }

  private:
    u32 mMask = 0;
    u32 mIrqState = 0;
};

}  // namespace

fl::shared_ptr<IRpBitBangPin> createRpBitBangDevicePin() FL_NO_EXCEPT {
    return fl::make_shared<RpBitBangDevicePin>();
}

u32 rpBitBangCpuHz() FL_NO_EXCEPT {
    return static_cast<u32>(clock_get_hz(clk_sys));
}

#endif  // FL_IS_RP2040 || FL_IS_RP2350

}  // namespace fl
