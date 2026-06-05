// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_LPC_H
#define __INC_CLOCKLESS_ARM_LPC_H

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC)

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

#endif  // FL_IS_ARM_LPC
#endif  // __INC_CLOCKLESS_ARM_LPC_H
