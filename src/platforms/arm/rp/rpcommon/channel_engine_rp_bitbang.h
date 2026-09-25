#pragma once

// IWYU pragma: private

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/is_rp.h"

namespace fl {

/// Convert a duration in nanoseconds to CPU cycles (floor). Computed once per
/// channel, never per bit. rpBitBangCycles(1250, 125000000) == 156.
constexpr u32 rpBitBangCycles(u32 ns, u32 cpuHz) FL_NO_EXCEPT {
    return static_cast<u32>((static_cast<u64>(ns) * static_cast<u64>(cpuHz)) /
                            static_cast<u64>(1000000000ull));
}

/// Pre-converted per-bit timing, in CPU cycles.
struct RpBitBangBitCycles {
    u32 t1 = 0;      ///< High time of a 0 bit.
    u32 t1t2 = 0;    ///< High time of a 1 bit.
    u32 period = 0;  ///< Full bit period (T1 + T2 + T3).
};

inline RpBitBangBitCycles rpBitBangBitCycles(const ChipsetTimingConfig& timing,
                                             u32 cpuHz) FL_NO_EXCEPT {
    RpBitBangBitCycles c;
    c.t1 = rpBitBangCycles(timing.t1_ns, cpuHz);
    c.t1t2 = rpBitBangCycles(timing.t1_ns + timing.t2_ns, cpuHz);
    c.period = rpBitBangCycles(timing.total_period_ns(), cpuHz);
    return c;
}

/// Injected pin sink. The device implementation drives SIO and busy-waits;
/// host tests record the edge schedule.
class IRpBitBangPin {
  public:
    virtual ~IRpBitBangPin() = default;
    /// Claim `pin` as an output driven low. Called once per channel.
    virtual void begin(int pin) FL_NO_EXCEPT { (void)pin; }
    /// Drive the pin to `high` and hold it for `holdCycles` CPU cycles.
    virtual void set(bool high, u32 holdCycles) FL_NO_EXCEPT = 0;
    /// Release after the channel's bits are sent.
    virtual void end() FL_NO_EXCEPT {}
    /// Hold the line low for the reset/latch time.
    virtual void latch(u32 resetUs) FL_NO_EXCEPT { (void)resetUs; }
};

/// Emit one bit: high for T1 (0) or T1+T2 (1), low for the rest of the period.
inline void rpBitBangEmitBit(IRpBitBangPin& sink, const RpBitBangBitCycles& c,
                             bool one) FL_NO_EXCEPT {
    const u32 high = one ? c.t1t2 : c.t1;
    sink.set(true, high);
    sink.set(false, c.period > high ? c.period - high : 0);
}

/// Emit the edge schedule for `len` bytes, MSB first, followed by
/// `extraZeroBits` zero bits after every byte.
inline void rpBitBangEmitBytes(IRpBitBangPin& sink, const RpBitBangBitCycles& c,
                               const u8* bytes, size_t len,
                               u8 extraZeroBits) FL_NO_EXCEPT {
    for (size_t i = 0; i < len; ++i) {
        const u8 b = bytes[i];
        for (int bit = 7; bit >= 0; --bit) {
            rpBitBangEmitBit(sink, c, ((b >> bit) & 1u) != 0);
        }
        for (u8 z = 0; z < extraZeroBits; ++z) {
            rpBitBangEmitBit(sink, c, false);
        }
    }
}

/// Blocking CPU bit-bang clockless engine: the M0 fallback used when
/// FASTLED_RP2040_CLOCKLESS_PIO=0 (#4635). show() transmits every enqueued
/// channel in order, then honors each channel's reset latch; poll() is READY
/// once show() returns.
class ChannelEngineRpBitBang final : public IChannelDriver {
  public:
    ChannelEngineRpBitBang(fl::shared_ptr<IRpBitBangPin> sink, u32 cpuHz,
                           const char* driver_name = "BIT_BANG") FL_NO_EXCEPT;
    ~ChannelEngineRpBitBang() override = default;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal(mDriverName);
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

  private:
    fl::shared_ptr<IRpBitBangPin> mSink;
    u32 mCpuHz;
    const char* mDriverName;
    fl::vector<ChannelDataPtr> mPendingChannels;
};

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
/// Device sink: SIO set/clr + calibrated busy-wait, interrupts off per frame.
fl::shared_ptr<IRpBitBangPin> createRpBitBangDevicePin() FL_NO_EXCEPT;
/// Current system clock in Hz (clock_get_hz(clk_sys)).
u32 rpBitBangCpuHz() FL_NO_EXCEPT;
#endif

}  // namespace fl
