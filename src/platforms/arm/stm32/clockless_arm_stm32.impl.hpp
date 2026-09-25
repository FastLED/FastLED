#pragma once

// IWYU pragma: private

/// @file clockless_arm_stm32.impl.hpp
/// @brief Template member definitions for clockless_arm_stm32.h.
///
/// Included only from clockless_arm_stm32.h, after the declarations.

#include "platforms/arm/stm32/clockless_arm_stm32.h"

namespace fl {

#define FL_STM32_CLOCKLESS_TPL template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0>
#define FL_STM32_CLOCKLESS_DRV ClocklessStm32Driver<DATA_PIN, TIMING, WAIT_TIME, XTRA0>

// ---- Shared ClocklessStm32Driver members (both cores) ----------------------

FL_STM32_CLOCKLESS_TPL
u32 FL_STM32_CLOCKLESS_DRV::typeId() FL_NO_EXCEPT {
    static const u32 id = stm32_detail::nextClocklessTypeId();
    return id;
}

FL_STM32_CLOCKLESS_TPL
bool FL_STM32_CLOCKLESS_DRV::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    return data && data->isClockless() && data->getPin() == DATA_PIN;
}

FL_STM32_CLOCKLESS_TPL
void FL_STM32_CLOCKLESS_DRV::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        mEnqueued.push_back(fl::move(channelData));
    }
}

FL_STM32_CLOCKLESS_TPL
IChannelDriver::DriverState FL_STM32_CLOCKLESS_DRV::poll() FL_NO_EXCEPT {
    return DriverState(DriverState::READY);
}

FL_STM32_CLOCKLESS_TPL
fl::string FL_STM32_CLOCKLESS_DRV::getName() const FL_NO_EXCEPT {
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

FL_STM32_CLOCKLESS_TPL
IChannelDriver::Capabilities FL_STM32_CLOCKLESS_DRV::getCapabilities() const FL_NO_EXCEPT {
    return Capabilities(true, false);
}

#if defined(FL_IS_ARM_M0_PLUS) || defined(FL_IS_ARM_M0) || defined(__ARM_ARCH_6M__)

// ---- M0/M0+ asm core ---------------------------------------------------------

FL_STM32_CLOCKLESS_TPL
void FL_STM32_CLOCKLESS_DRV::show() FL_NO_EXCEPT {
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

FL_STM32_CLOCKLESS_TPL
void FL_STM32_CLOCKLESS_DRV::initData(M0ClocklessData* data) FL_NO_EXCEPT {
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

FL_STM32_CLOCKLESS_TPL
u32 FL_STM32_CLOCKLESS_DRV::sendBytes(const u8* bytes, u32 len) FL_NO_EXCEPT {
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

#else

// ---- Cortex-M3/M4/M7 DWT cycle-counter core ----------------------------------

FL_STM32_CLOCKLESS_TPL
void FL_STM32_CLOCKLESS_DRV::show() FL_NO_EXCEPT {
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

FL_STM32_CLOCKLESS_TPL
void FL_STM32_CLOCKLESS_DRV::showBytes(const u8* bytes, u32 len) FL_NO_EXCEPT {
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

FL_STM32_CLOCKLESS_TPL
template<int BITS>
inline void FL_STM32_CLOCKLESS_DRV::writeBits(
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

FL_STM32_CLOCKLESS_TPL
u32 FL_STM32_CLOCKLESS_DRV::showRGBInternal(
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
    FASTLED_REGISTER data_t hi = *port | FastPin<DATA_PIN>::mask();
    FASTLED_REGISTER data_t lo = *port & ~FastPin<DATA_PIN>::mask();
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

#endif

// ---- ClocklessStm32Traits ---------------------------------------------------

FL_STM32_CLOCKLESS_TPL
typename ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::Driver
    ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::sDriver;

FL_STM32_CLOCKLESS_TPL
fl::shared_ptr<typename ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::Driver>
ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::instancePtr() FL_NO_EXCEPT {
    return fl::make_shared_no_tracking(sDriver);
}

FL_STM32_CLOCKLESS_TPL
IChannelDriver& ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::instance() FL_NO_EXCEPT {
    return sDriver;
}

FL_STM32_CLOCKLESS_TPL
void ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>::registerWithManager() FL_NO_EXCEPT {
    ChannelManager& manager = ChannelManager::registry();
    if (manager.findDriverByName(sDriver.getName())) {
        return;
    }
    manager.addDriver(0, instancePtr());
}

// ---- ClocklessController -----------------------------------------------------

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER, int XTRA0, bool FLIP, int WAIT_TIME>
u16 ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>::getMaxRefreshRate() const FL_NO_EXCEPT {
    return 400;
}

#undef FL_STM32_CLOCKLESS_DRV
#undef FL_STM32_CLOCKLESS_TPL

}  // namespace fl
