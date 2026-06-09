// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_LPC_H
#define __INC_CLOCKLESS_ARM_LPC_H

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "fastled_delay.h"

// When the PLU opt-in is set on an LPC804 build, the dedicated PLU driver
// (clockless_arm_lpc_plu.h) provides the ClocklessController specialisation;
// compile this bit-banged driver out so we don't double-define the template.
//
// Scoped to LPC chips that share the "modern" 0xA0000000 GPIO controller
// (LPC845, LPC804, LPC11U24/U35, LPC15xx — see fastpin_arm_lpc.h). The
// FL_LPC_HI_OFFSET / FL_LPC_LO_OFFSET constants below are byte offsets in
// that controller's register layout and apply uniformly.
//
// On LPC11xx legacy parts (LPC1110/1112/1114/1115) the GPIO is at
// 0x50000000 with 12-bit masked-access semantics — that family needs its
// own clockless driver, tracked in #2845 Stage 4.
#if (defined(FL_LPC845) || defined(FL_LPC804) || \
     defined(FL_LPC11_USB) || defined(FL_LPC15)) && \
    !(defined(FL_LPC804) && defined(FASTLED_LPC_PLU))

#include "platforms/arm/common/m0clockless.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "eorder.h"

namespace fl {

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

// Byte offsets from FastPin<>::port() (which points at LPC_GPIO->PIN[0]) to
// the SET / CLR registers used by the shared M0 clockless C++ implementation.
//   PIN[0]   @ controller base + 0x2100  (port base for FastPin)
//   SET[0]   @ controller base + 0x2200  -> +0x100 from PIN[0]
//   CLR[0]   @ controller base + 0x2280  -> +0x180 from PIN[0]
#define FL_LPC_HI_OFFSET 0x100
#define FL_LPC_LO_OFFSET 0x180

template <u8 DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t     data_t;

    data_t     mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

public:
    virtual void init() FL_NOEXCEPT {
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort    = FastPin<DATA_PIN>::port();
    }

    virtual u16 getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER>& pixels) FL_NOEXCEPT {
        mWait.wait();
        cli();
        if (!showRGBInternal(pixels)) {
            sei();
            delayMicroseconds(WAIT_TIME);
            cli();
            showRGBInternal(pixels);
        }
        sei();
        mWait.mark();
    }

    static u32 showRGBInternal(PixelController<RGB_ORDER> pixels) FL_NOEXCEPT {
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
        data.adj  = pixels.mAdvance;

        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        return showLedData<FL_LPC_HI_OFFSET, FL_LPC_LO_OFFSET, TIMING, RGB_ORDER, WAIT_TIME>(
            portBase, FastPin<DATA_PIN>::mask(), pixels.mData, pixels.mLen, &data);
    }
};

}  // namespace fl

#endif  // (FL_LPC845 || FL_LPC804) && !(FL_LPC804 && FASTLED_LPC_PLU)
#endif  // __INC_CLOCKLESS_ARM_LPC_H
