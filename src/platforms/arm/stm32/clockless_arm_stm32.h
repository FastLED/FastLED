#pragma once

// IWYU pragma: private

/// @file clockless_arm_stm32.h
/// @brief STM32 legacy clockless controller routed through the slim bridge.
///
/// `ClocklessController` is a `fl::SlimBridgeController` (issue #4594): the
/// legacy `addLeds<>()` path encodes pixels into a `ChannelData` buffer and
/// hands it to a per-pin `ClocklessStm32Driver`. The driver keeps the
/// existing cycle-counted bit-bang cores (M0 asm core from `m0clockless.h`,
/// DWT/CYCCNT loop on Cortex-M3/M4/M7) as the byte-emitting engine.
///
/// The driver registers itself with `ChannelManager::registry()` from the
/// bridge constructor, so it is linked only when a sketch instantiates a
/// clockless controller (#4630 used-driver-only linking).

#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/has_include.h"
#include "eorder.h"
#include "fastled_delay.h"
#include "platforms/arm/stm32/interrupts_stm32_inline.h"
#include "platforms/arm/stm32/core_detection.h"
#include "platforms/arm/stm32/is_stm32.h"

// Get CMSIS DWT/CoreDebug registers from framework or fallback
#if FL_HAS_INCLUDE("stm32_def.h")
    // STM32duino core - stm32_def.h includes device header which includes core_cmX.h
    // IWYU pragma: begin_keep
    #include <stm32_def.h>
    // IWYU pragma: end_keep
#else
    // Reuse already included CMSIS (e.g. Zephyr); otherwise use libmaple fallback.
    #include "platforms/arm/stm32/cm3_regs.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
#endif

#include "platforms/arm/is_arm.h"
#include "platforms/arm/common/m0clockless.h"

namespace fl {

namespace stm32_detail {
// Hands out a distinct id per ClocklessStm32Driver specialization.
inline u32 nextClocklessTypeId() FL_NO_EXCEPT {
    static u32 sNext = 0;
    return ++sNext;
}
}  // namespace stm32_detail
// Definition for a single channel clockless controller for the stm32 family of chips, like that used in the spark core
// See clockless.h for detailed info on how the template parameters are used.

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#if defined(FL_IS_ARM_M0_PLUS) || defined(FL_IS_ARM_M0) || defined(__ARM_ARCH_6M__)

/// @brief Blocking single-pin clockless driver for STM32 M0/M0+ (asm core).
///
/// The input buffer is already colour-ordered, scaled and dithered by the
/// bridge's `PixelIterator`, so the asm core runs with identity scale and
/// zero dither and simply shifts the bytes out.
template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0 = 0>
class ClocklessStm32Driver : public IChannelDriver {
    // Stable id unique to this template specialization.
    static u32 typeId() FL_NO_EXCEPT {
        static const u32 id = stm32_detail::nextClocklessTypeId();
        return id;
    }

public:
    ClocklessStm32Driver() FL_NO_EXCEPT : mPinReady(false) {}

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
            fl::interruptsDisable();
            if (!sendBytes(bytes.data(), static_cast<u32>(bytes.size()))) {
                fl::interruptsEnable();
                delayMicroseconds(WAIT_TIME);
                fl::interruptsDisable();
                sendBytes(bytes.data(), static_cast<u32>(bytes.size()));
            }
            fl::interruptsEnable();
            mWait.mark();
            ch->setInUse(false);
        }
        mEnqueued.clear();
    }

    DriverState poll() FL_NO_EXCEPT override {
        return DriverState(DriverState::READY);
    }

    fl::string getName() const FL_NO_EXCEPT override {
        // Unique per template specialization so controllers sharing a pin
        // with different timings keep separate drivers.
        fl::string name = fl::string::from_literal("STM32_CLOCKLESS_P");
        name.append(static_cast<i32>(DATA_PIN));
        name.append("_T");
        name.append(static_cast<i32>(TIMING::T1));
        name.append("_");
        name.append(static_cast<i32>(TIMING::T2));
        name.append("_");
        name.append(static_cast<i32>(TIMING::T3));
        name.append("_R");
        name.append(static_cast<i32>(TIMING::RESET));
        name.append("_W");
        name.append(static_cast<i32>(WAIT_TIME));
        name.append("_X");
        name.append(static_cast<i32>(XTRA0));
        // TIMING types with identical numeric values are still distinct
        // specializations with distinct static drivers; a
        // per-specialization type id makes the registration key unique.
        name.append("_#");
        name.append(typeId());
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
    /// trailing 1-2 byte remainder (RGBW strips) is zero-padded.
    /// @return 0 if an interrupt overran the frame, nonzero on success.
    static u32 sendBytes(const u8* bytes, u32 len) FL_NO_EXCEPT {
        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        const u32 mask = FastPin<DATA_PIN>::mask();
        const u32 groups = len / 3;
        const u32 rem = len - groups * 3;
        M0ClocklessData data;
        if (groups > 0) {
            initData(&data);
            if (!showLedData<4, 20, TIMING, RGB, WAIT_TIME>(portBase, mask, bytes, groups, &data)) {
                return 0;
            }
        }
        if (rem > 0) {
            u8 tail[3] = {0, 0, 0};
            for (u32 i = 0; i < rem; ++i) {
                tail[i] = bytes[groups * 3 + i];
            }
            initData(&data);
            if (!showLedData<4, 20, TIMING, RGB, WAIT_TIME>(portBase, mask, tail, 1, &data)) {
                return 0;
            }
        }
        return 1;
    }

    fl::vector<ChannelDataPtr> mEnqueued;
    CMinWait<WAIT_TIME> mWait;
    bool mPinReady;
};

#else

#if defined(FL_IS_STM32_F2)
// The photon runs faster than the others
#define ADJ 8
#else
#define ADJ 20
#endif

/// @brief Blocking single-pin clockless driver for STM32 Cortex-M3/M4/M7
/// (DWT cycle-counter bit-bang core).
///
/// The input buffer is already colour-ordered, scaled and dithered (and RGBW
/// expanded) by the bridge's `PixelIterator`; the core only shifts bytes out.
template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0 = 0>
class ClocklessStm32Driver : public IChannelDriver {
    // Stable id unique to this template specialization.
    static u32 typeId() FL_NO_EXCEPT {
        static const u32 id = stm32_detail::nextClocklessTypeId();
        return id;
    }

    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t data_t;

public:
    ClocklessStm32Driver() FL_NO_EXCEPT : mPinReady(false) {}

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
            showBytes(bytes.data(), static_cast<u32>(bytes.size()));
            ch->setInUse(false);
        }
        mEnqueued.clear();
    }

    DriverState poll() FL_NO_EXCEPT override {
        return DriverState(DriverState::READY);
    }

    fl::string getName() const FL_NO_EXCEPT override {
        // Unique per template specialization so controllers sharing a pin
        // with different timings keep separate drivers.
        fl::string name = fl::string::from_literal("STM32_CLOCKLESS_P");
        name.append(static_cast<i32>(DATA_PIN));
        name.append("_T");
        name.append(static_cast<i32>(TIMING::T1));
        name.append("_");
        name.append(static_cast<i32>(TIMING::T2));
        name.append("_");
        name.append(static_cast<i32>(TIMING::T3));
        name.append("_R");
        name.append(static_cast<i32>(TIMING::RESET));
        name.append("_W");
        name.append(static_cast<i32>(WAIT_TIME));
        name.append("_X");
        name.append(static_cast<i32>(XTRA0));
        // TIMING types with identical numeric values are still distinct
        // specializations with distinct static drivers; a
        // per-specialization type id makes the registration key unique.
        name.append("_#");
        name.append(typeId());
        return name;
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

private:
    void showBytes(const u8* bytes, u32 len) FL_NO_EXCEPT {
        mWait.wait();

        // Compute timing from actual CPU frequency (just-in-time per-frame calculation)
        // F_CPU is runtime SystemCoreClock on STM32duino, compile-time constant elsewhere
        u32 cpu_freq = F_CPU;

        // Convert nanoseconds to clock cycles: cycles = nanoseconds * frequency / 1e9
        // Use u64 to avoid overflow (e.g., 900ns * 180MHz = 162 billion)
        u32 t1_clocks = static_cast<u64>(TIMING::T1) * cpu_freq / 1000000000ULL;
        u32 t2_clocks = static_cast<u64>(TIMING::T2) * cpu_freq / 1000000000ULL;
        u32 t3_clocks = static_cast<u64>(TIMING::T3) * cpu_freq / 1000000000ULL;

        // Clocks per microsecond for interrupt timeout checks
        u32 clks_per_us = cpu_freq / 1000000;

        if(!showRGBInternal(bytes, len, t1_clocks, t2_clocks, t3_clocks, clks_per_us)) {
            // showRGBInternal already re-enabled interrupts before returning 0
            delayMicroseconds(WAIT_TIME);
            fl::interruptsDisable(); // Disable interrupts for retry
            showRGBInternal(bytes, len, t1_clocks, t2_clocks, t3_clocks, clks_per_us);
        }
        mWait.mark();
    }

#define _CYCCNT (*(volatile u32*)(0xE0001004UL))

    template<int BITS> __attribute__ ((always_inline))
    inline static void writeBits(
        FASTLED_REGISTER u32 & next_mark, FASTLED_REGISTER data_ptr_t port,
        FASTLED_REGISTER data_t hi, FASTLED_REGISTER data_t lo, FASTLED_REGISTER u8 & b,
        u32 t1_clocks, u32 t1t2_clocks, u32 t1t2t3_clocks) FL_NO_EXCEPT {
        for(FASTLED_REGISTER u32 i = BITS-1; i > 0; --i) {
            while(_CYCCNT < (t1t2t3_clocks-ADJ));
            FastPin<DATA_PIN>::fastset(port, hi);
            _CYCCNT = 4;
            if(b&0x80) {
                while(_CYCCNT < (t1t2_clocks-ADJ));
                FastPin<DATA_PIN>::fastset(port, lo);
            } else {
                while(_CYCCNT < (t1_clocks-ADJ/2));
                FastPin<DATA_PIN>::fastset(port, lo);
            }
            b <<= 1;
        }

        while(_CYCCNT < (t1t2t3_clocks-ADJ));
        FastPin<DATA_PIN>::fastset(port, hi);
        _CYCCNT = 4;

        if(b&0x80) {
            while(_CYCCNT < (t1t2_clocks-ADJ));
            FastPin<DATA_PIN>::fastset(port, lo);
        } else {
            while(_CYCCNT < (t1_clocks-ADJ/2));
            FastPin<DATA_PIN>::fastset(port, lo);
        }
    }

    static u32 showRGBInternal(
            const u8* bytes, u32 len,
            u32 t1_clocks, u32 t2_clocks, u32 t3_clocks, u32 clks_per_us) FL_NO_EXCEPT {
        (void)clks_per_us;
        // Pre-calculate combined timing values for the hot loop
        const u32 t1t2_clocks = t1_clocks + t2_clocks;
        const u32 t1t2t3_clocks = t1t2_clocks + t3_clocks;

        // Get access to the clock if available (Cortex-M3/M4/M7/M33)
#if defined(DCB) && defined(DWT)
        DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
#elif defined(CoreDebug) && defined(DWT)
        CoreDebug->DEMCR  |= CoreDebug_DEMCR_TRCENA_Msk;
#endif
#if defined(DWT)
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        DWT->CYCCNT = 0;
#endif

        FASTLED_REGISTER data_ptr_t port = FastPin<DATA_PIN>::port();
        FASTLED_REGISTER data_t hi = *port | FastPin<DATA_PIN>::mask();;
        FASTLED_REGISTER data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
        *port = lo;

        fl::interruptsDisable();

        u32 next_mark = t1t2t3_clocks;

#if defined(DWT)
        DWT->CYCCNT = 0;
#endif

        #if (FASTLED_ALLOW_INTERRUPTS == 1)
        bool first_pixel = true;
        #endif

        // The bridge hands over pre-encoded bytes (3 per RGB pixel, 4 per
        // RGBW pixel). The interrupt window opens between 3-byte groups,
        // matching the legacy per-pixel window for RGB strips.
        u32 pos = 0;
        while(pos < len) {
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            // Only disable after the first pixel, since we already disabled interrupts initially
            if (!first_pixel) {
                fl::interruptsDisable();
            }
            first_pixel = false;
            // if interrupts took longer than 45µs, punt on the current frame
#if defined(DWT)
            if(DWT->CYCCNT > next_mark) {
                if((DWT->CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*clks_per_us)) {
                    fl::interruptsEnable(); return 0;
                }
            }
#endif

            hi = *port | FastPin<DATA_PIN>::mask();
            lo = *port & ~FastPin<DATA_PIN>::mask();
            #endif

            u32 end = pos + 3;
            if (end > len) {
                end = len;
            }
            for (; pos < end; ++pos) {
                u8 b = bytes[pos];
                writeBits<8 + XTRA0>(next_mark, port, hi, lo, b, t1_clocks, t1t2_clocks, t1t2t3_clocks);
            }

            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            fl::interruptsEnable();
            #endif
        }

        #if (FASTLED_ALLOW_INTERRUPTS == 0)
        // Only need final enable if interrupts weren't re-enabled in the loop
        fl::interruptsEnable();
        #endif
#if defined(DWT)
        return DWT->CYCCNT;
#else
        return 1;  // nonzero means success
#endif
    }

    fl::vector<ChannelDataPtr> mEnqueued;
    CMinWait<WAIT_TIME> mWait;
    bool mPinReady;
};

#endif

/// @brief Driver traits for `SlimBridgeController` (one driver per
/// pin/timing/wait/XTRA0 specialization; never shared across timings).
template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0>
struct ClocklessStm32Traits {
    using Driver = ClocklessStm32Driver<DATA_PIN, TIMING, WAIT_TIME, XTRA0>;

    /// Storage for this specialization's driver: a static member (no
    /// function-local static guard), handed out as a no-tracking shared_ptr.
    static Driver sDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        return fl::make_shared_no_tracking(sDriver);
    }

    static IChannelDriver& instance() FL_NO_EXCEPT { return sDriver; }

    /// Idempotent: the driver name is unique per specialization, so this only
    /// skips re-registering this exact driver.
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager& manager = ChannelManager::registry();
        if (manager.findDriverByName(sDriver.getName())) {
            return;
        }
        manager.addDriver(0, instancePtr());
    }
};

template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0>
typename ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::Driver ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::sDriver;

/// @brief STM32 Clockless LED Controller (slim bridge, issue #4594)
/// @tparam DATA_PIN Pin number for data line output
/// @tparam TIMING ChipsetTiming structure containing T1, T2, T3, and RESET values
/// @tparam RGB_ORDER Color order (RGB, GRB, etc.)
/// @tparam XTRA0 Extra trailing zero bits sent after each byte
/// @tparam FLIP Flip the output bit order if true (unused)
/// @tparam WAIT_TIME Wait time between updates in microseconds
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME,
                                  ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>, XTRA0> {
public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};

}  // namespace fl

FL_DISABLE_WARNING_POP
