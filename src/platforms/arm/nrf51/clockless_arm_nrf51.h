// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_NRF51
#define __INC_CLOCKLESS_ARM_NRF51

#include "platforms/arm/nrf51/is_nrf51.h"
#include "platforms/arm/nrf51/fastpin_arm_nrf51.h"  // Nrf51HwPin

#if defined(FL_IS_NRF51)

// IWYU pragma: begin_keep
#include <nrf51_bitfields.h>
// IWYU pragma: end_keep

#if (FASTLED_ALLOW_INTERRUPTS==1)
#define SEI_CHK LED_TIMER->CC[0] = (WAIT_TIME * (F_CPU/1000000)); LED_TIMER->TASKS_CLEAR; LED_TIMER->EVENTS_COMPARE[0] = 0;
#define CLI_CHK cli(); if(LED_TIMER->EVENTS_COMPARE[0]) { LED_TIMER->TASKS_STOP = 1; return 0; }
#define INNER_SEI sei();
#else
#define SEI_CHK
#define CLI_CHK
#define INNER_SEI delaycycles<1>();
#endif


#include "platforms/arm/common/m0clockless.h"
#include "fl/chipsets/timing_traits.h"
#include "eorder.h"
#include "fastled_delay.h"
#include "fl/stl/noexcept.h"

namespace fl {
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

template <fl::u8 DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 75>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    typedef typename Nrf51HwPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename Nrf51HwPin<DATA_PIN>::port_t data_t;

    data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

public:
    virtual void init() FL_NO_EXCEPT {
        Nrf51HwPin<DATA_PIN>::setOutput();
        mPinMask = Nrf51HwPin<DATA_PIN>::mask();
        mPort = Nrf51HwPin<DATA_PIN>::port();
    }

	virtual fl::u16 getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER> & pixels) FL_NO_EXCEPT {
        mWait.wait();
        cli();
        if(!showRGBInternal(pixels)) {
            sei(); delayMicroseconds(WAIT_TIME); cli();
            showRGBInternal(pixels);
        }
        sei();
        mWait.mark();
    }

    // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
    // gcc will use register Y for the this pointer.
    static fl::u32 showRGBInternal(PixelController<RGB_ORDER> pixels) FL_NO_EXCEPT {
        struct M0ClocklessData data;
        data.d[0] = pixels.d[0];
        data.d[1] = pixels.d[1];
        data.d[2] = pixels.d[2];
        data.s[0] = pixels.mColorAdjustment.premixed[0];
        data.s[1] = pixels.mColorAdjustment.premixed[1];
        data.s[2] = pixels.mColorAdjustment.premixed[2];
        data.e[0] = pixels.e[0];
        data.e[1] = pixels.e[1];
        data.e[2] = pixels.e[2];
        data.adj = pixels.mAdvance;

        typename Nrf51HwPin<DATA_PIN>::port_ptr_t portBase = Nrf51HwPin<DATA_PIN>::port();

        // timer mode w/prescaler of 0
        LED_TIMER->MODE = TIMER_MODE_MODE_Timer;
        LED_TIMER->PRESCALER = 0;
        LED_TIMER->EVENTS_COMPARE[0] = 0;
        LED_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
        LED_TIMER->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
        LED_TIMER->TASKS_START = 1;

        int ret = showLedData<4,8,TIMING,RGB_ORDER,WAIT_TIME>(portBase, Nrf51HwPin<DATA_PIN>::mask(), pixels.mData, pixels.mLen, &data);

        LED_TIMER->TASKS_STOP = 1;
        return ret; // 0x00FFFFFF - _VAL;
    }
};

}  // namespace fl


#endif // FL_IS_NRF51
#endif // __INC_CLOCKLESS_ARM_NRF51
