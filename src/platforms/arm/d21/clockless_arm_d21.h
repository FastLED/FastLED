// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_D21
#define __INC_CLOCKLESS_ARM_D21

/// @file clockless_arm_d21.h
/// @brief SAMD21 legacy clockless controller routed through the slim bridge.
///
/// `ClocklessController` is a `fl::SlimBridgeController` (issue #4593): the
/// legacy `addLeds<>()` path encodes pixels into a `ChannelData` buffer and
/// hands it to a per-pin `ClocklessSamd21Driver`. That driver keeps the
/// cycle-accurate M0/M0+ core from `platforms/arm/common/m0clockless.h` as
/// the byte-emitting engine: the shared `BitBangChannelDriver` cannot hit
/// WS281x timing on a 48 MHz M0+, so the existing `showLedData` loop is
/// wrapped as an `IChannelDriver` instead.
///
/// The driver registers itself with `ChannelManager::registry()` from the
/// bridge constructor, so it is linked only when a sketch instantiates a
/// clockless controller (#4630 used-driver-only linking).

#include "platforms/arm/common/m0clockless.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "eorder.h"
#include "fastled_delay.h"
#include "fl/stl/noexcept.h"

namespace fl {
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

/// @brief Blocking single-pin clockless driver for SAMD21 (M0+ asm core).
///
/// The input buffer is already colour-ordered, scaled and dithered by the
/// bridge's `PixelIterator`, so the asm core runs with identity scale and
/// zero dither and simply shifts the bytes out.
template <int DATA_PIN, typename TIMING, int WAIT_TIME>
class ClocklessSamd21Driver : public IChannelDriver {
public:
    ClocklessSamd21Driver() FL_NO_EXCEPT : mPinReady(false) {}

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override {
        return data && data->isClockless() && data->getPin() == DATA_PIN;
    }

    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override {
        if (channelData) {
            mEnqueued.push_back(fl::move(channelData));
        }
    }

    void show() FL_NO_EXCEPT override {
        if (mEnqueued.empty()) {
            return;
        }
        if (!mPinReady) {
            FastPin<DATA_PIN>::setOutput();
            mPinReady = true;
        }
        for (fl::size i = 0; i < mEnqueued.size(); ++i) {
            const ChannelDataPtr& ch = mEnqueued[i];
            if (!ch) {
                continue;
            }
            ch->setInUse(true);
            const fl::vector_psram<u8>& bytes = ch->getData();
            mWait.wait();
            cli();
            if (!sendBytes(bytes.data(), static_cast<u32>(bytes.size()))) {
                sei(); delayMicroseconds(WAIT_TIME); cli();
                sendBytes(bytes.data(), static_cast<u32>(bytes.size()));
            }
            sei();
            mWait.mark();
            ch->setInUse(false);
        }
        mEnqueued.clear();
    }

    DriverState poll() FL_NO_EXCEPT override {
        return DriverState(DriverState::READY);
    }

    fl::string getName() const FL_NO_EXCEPT override {
        fl::string name = fl::string::from_literal("SAMD21_CLOCKLESS_P");
        name.append(static_cast<i32>(DATA_PIN));
        return name;
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

private:
    static void initData(M0ClocklessData* data) FL_NO_EXCEPT {
        for (int i = 0; i < 3; ++i) {
            data->d[i] = 0;
            data->e[i] = 0;
            // Identity scale: showLedData pre-increments s when
            // FASTLED_SCALE8_FIXED, so 255 becomes 256 (x*256>>8 == x).
#if (FASTLED_SCALE8_FIXED == 1)
            data->s[i] = 255;
#else
            data->s[i] = 256;
#endif
        }
        data->adj = 3;
        data->pad = 0;
    }

    /// Emit `len` raw bytes. The asm core consumes 3-byte groups, so a
    /// trailing 1-2 byte remainder (RGBW strips) is zero-padded; the extra
    /// zero bits fall off the end of the strip.
    /// @return 0 if an interrupt overran the frame, nonzero on success.
    static u32 sendBytes(const u8* bytes, u32 len) FL_NO_EXCEPT {
        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        const u32 mask = FastPin<DATA_PIN>::mask();
        const u32 groups = len / 3;
        const u32 rem = len - groups * 3;
        M0ClocklessData data;
        if (groups > 0) {
            initData(&data);
            if (!showLedData<8, 4, TIMING, RGB, WAIT_TIME>(portBase, mask, bytes, groups, &data)) {
                return 0;
            }
        }
        if (rem > 0) {
            u8 tail[3] = {0, 0, 0};
            for (u32 i = 0; i < rem; ++i) {
                tail[i] = bytes[groups * 3 + i];
            }
            initData(&data);
            if (!showLedData<8, 4, TIMING, RGB, WAIT_TIME>(portBase, mask, tail, 1, &data)) {
                return 0;
            }
        }
        return 1;
    }

    fl::vector<ChannelDataPtr> mEnqueued;
    CMinWait<WAIT_TIME> mWait;
    bool mPinReady;
};

/// @brief Driver traits for `SlimBridgeController` (per pin/timing singleton).
template <int DATA_PIN, typename TIMING, int WAIT_TIME>
struct ClocklessSamd21Traits {
    using Driver = ClocklessSamd21Driver<DATA_PIN, TIMING, WAIT_TIME>;

    /// Storage for this pin/timing's driver: a static member (no function-local
    /// static guard), handed out as a no-tracking shared_ptr.
    static Driver sDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        return fl::make_shared_no_tracking(sDriver);
    }

    /// The driver actually used for this pin: the first one registered under
    /// the pin-only name, so every timing specialization on the same pin queues
    /// frames to the single registered driver.
    static IChannelDriver& instance() FL_NO_EXCEPT {
        fl::shared_ptr<IChannelDriver> existing =
            ChannelManager::registry().findDriverByName(sDriver.getName());
        if (existing) {
            return *existing;
        }
        return sDriver;
    }

    /// Idempotent: skip if a driver for this pin is already registered (by any
    /// timing specialization), so a second controller does not replace it.
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager& manager = ChannelManager::registry();
        if (manager.findDriverByName(sDriver.getName())) {
            return;
        }
        manager.addDriver(0, instancePtr());
    }
};

template <int DATA_PIN, typename TIMING, int WAIT_TIME>
typename ClocklessSamd21Traits<DATA_PIN, TIMING, WAIT_TIME>::Driver ClocklessSamd21Traits<DATA_PIN, TIMING, WAIT_TIME>::sDriver;

/// @brief ARM D21 (SAMD21) Clockless LED Controller
/// @tparam DATA_PIN Pin number for data line output
/// @tparam TIMING ChipsetTiming structure containing T1, T2, T3, and RESET values
/// @tparam RGB_ORDER Color order (RGB, GRB, etc.)
/// @tparam XTRA0 Additional parameter for platform-specific needs (unused by the M0 core)
/// @tparam FLIP Flip the output bit order if true (unused)
/// @tparam WAIT_TIME Wait time between updates in microseconds
///
/// Example usage with named timing constant:
/// @code
///   ClocklessController<5, TIMING_WS2812_800KHZ, GRB> controller;
/// @endcode
template <u8 DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME,
                                  ClocklessSamd21Traits<DATA_PIN, TIMING, WAIT_TIME>, XTRA0> {
public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};

}  // namespace fl
#endif // __INC_CLOCKLESS_ARM_D21
