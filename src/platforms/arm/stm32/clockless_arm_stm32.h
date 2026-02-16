#pragma once

// IWYU pragma: private

#include "fl/chipsets/timing_traits.h"
#include "fl/stl/vector.h"
#include "fl/has_include.h"
#include "fastled_delay.h"
#include "platforms/arm/stm32/interrupts_stm32_inline.h"
#include "platforms/arm/stm32/core_detection.h"
#include "platforms/arm/stm32/is_stm32.h"

// Get CMSIS DWT/CoreDebug registers from framework or fallback
#if FL_HAS_INCLUDE("stm32_def.h")
    // STM32duino core - stm32_def.h includes device header which includes core_cmX.h
    #include <stm32_def.h>
#else
    // Libmaple or other cores without CMSIS - use our definitions
    #include "platforms/arm/stm32/cm3_regs.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
#endif

namespace fl {
// Definition for a single channel clockless controller for the stm32 family of chips, like that used in the spark core
// See clockless.h for detailed info on how the template parameters are used.

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#if defined(FL_IS_STM32_F2)
// The photon runs faster than the others
#define ADJ 8
#else
#define ADJ 20
#endif

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t data_t;

    data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

public:
    virtual void init() {
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort = FastPin<DATA_PIN>::port();
    }

    virtual u16 getMaxRefreshRate() const { return 400; }

protected:
    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
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

        Rgbw rgbw = this->getRgbw();
        if(!showRGBInternal(pixels, rgbw, t1_clocks, t2_clocks, t3_clocks, clks_per_us)) {
            // showRGBInternal already re-enabled interrupts before returning 0
            delayMicroseconds(WAIT_TIME);
            fl::interruptsDisable(); // Disable interrupts for retry
            showRGBInternal(pixels, rgbw, t1_clocks, t2_clocks, t3_clocks, clks_per_us);
        }
        mWait.mark();
    }

#define _CYCCNT (*(volatile u32*)(0xE0001004UL))

    template<int BITS> __attribute__ ((always_inline))
    inline static void writeBits(
        FASTLED_REGISTER u32 & next_mark, FASTLED_REGISTER data_ptr_t port,
        FASTLED_REGISTER data_t hi, FASTLED_REGISTER data_t lo, FASTLED_REGISTER u8 & b,
        u32 t1_clocks, u32 t1t2_clocks, u32 t1t2t3_clocks)  {
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
            PixelController<RGB_ORDER> pixels, Rgbw rgbw,
            u32 t1_clocks, u32 t2_clocks, u32 t3_clocks, u32 clks_per_us) {
        // Pre-calculate combined timing values for the hot loop
        const u32 t1t2_clocks = t1_clocks + t2_clocks;
        const u32 t1t2t3_clocks = t1t2_clocks + t3_clocks;

        // Get access to the clock
        CoreDebug->DEMCR  |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        DWT->CYCCNT = 0;

        FASTLED_REGISTER data_ptr_t port = FastPin<DATA_PIN>::port();
        FASTLED_REGISTER data_t hi = *port | FastPin<DATA_PIN>::mask();;
        FASTLED_REGISTER data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
        *port = lo;

        fl::interruptsDisable();

        u32 next_mark = t1t2t3_clocks;

        DWT->CYCCNT = 0;

        // Detect RGBW mode using pattern from RP2040 drivers
        const bool is_rgbw = rgbw.active();

        // Unified loop using fixed-size buffer for both RGB and RGBW
        pixels.preStepFirstByteDithering();
        #if (FASTLED_ALLOW_INTERRUPTS == 1)
        bool first_pixel = true;
        #endif

        while(pixels.has(1)) {
            pixels.stepDithering();
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            // Only disable after the first pixel, since we already disabled interrupts initially
            if (!first_pixel) {
                fl::interruptsDisable();
            }
            first_pixel = false;
            // if interrupts took longer than 45µs, punt on the current frame
            if(DWT->CYCCNT > next_mark) {
                if((DWT->CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*clks_per_us)) {
                    fl::interruptsEnable(); return 0;\
                }
            }

            hi = *port | FastPin<DATA_PIN>::mask();
            lo = *port & ~FastPin<DATA_PIN>::mask();
            #endif

            // Load bytes into fixed buffer (3 for RGB, 4 for RGBW)
            fl::vector_fixed<u8, 4> bytes;
            if (is_rgbw) {
                u8 b0, b1, b2, b3;
                pixels.loadAndScaleRGBW(rgbw, &b0, &b1, &b2, &b3);
                bytes.push_back(b0);
                bytes.push_back(b1);
                bytes.push_back(b2);
                bytes.push_back(b3);
            } else {
                bytes.push_back(pixels.loadAndScale0());
                bytes.push_back(pixels.loadAndScale1());
                bytes.push_back(pixels.loadAndScale2());
            }

            // Write all bytes
            for (fl::size i = 0; i < bytes.size(); ++i) {
                writeBits<8+XTRA0>(next_mark, port, hi, lo, bytes[i], t1_clocks, t1t2_clocks, t1t2t3_clocks);
            }

            pixels.advanceData();
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            fl::interruptsEnable();
            #endif
        }

        #if (FASTLED_ALLOW_INTERRUPTS == 0)
        // Only need final enable if interrupts weren't re-enabled in the loop
        fl::interruptsEnable();
        #endif
        return DWT->CYCCNT;
    }
};

}  // namespace fl

FL_DISABLE_WARNING_POP
