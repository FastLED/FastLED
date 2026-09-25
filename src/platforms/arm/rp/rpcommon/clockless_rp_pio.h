// IWYU pragma: private

#ifndef __INC_CLOCKLESS_RP_PIO_COMMON
#define __INC_CLOCKLESS_RP_PIO_COMMON

#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/rp/is_rp.h"  // FL_IS_RP2040, FL_IS_RP2350

#if FASTLED_RP2040_CLOCKLESS_PIO
#include "fl/channels/bus.h"
#include "fl/channels/slim_bridge_controller.h"
#include "platforms/arm/rp/rpcommon/rp_pio_tx_bus_traits.h"
#else
// IWYU pragma: begin_keep
#include "hardware/structs/sio.h"
// IWYU pragma: end_keep
#include "fastled_delay.h"
#include "platforms/arm/common/m0clockless.h"
#endif

namespace fl {
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#if FASTLED_RP2040_CLOCKLESS_PIO

// Legacy `addLeds<>()` clockless controller for RP2040/RP2350. It is a slim
// bridge (issue #4589): the controller owns one ChannelData, re-encodes it
// every frame and enqueues it on the PIO channel engine
// (`BusTraits<Bus::FLEX_IO, 0>` == ChannelEngineRpPio on PIO0). Legacy strips
// and Channel API strips therefore share one PIO/DMA owner instead of two.
// XTRA0 and FLIP are accepted for source compatibility and ignored.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessRpPio
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::FLEX_IO, 0>> {
  public:
    ClocklessRpPio() FL_NO_EXCEPT = default;

    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessController = ClocklessRpPio<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

#else  // !FASTLED_RP2040_CLOCKLESS_PIO: blocking M0 bit-bang controller

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    CMinWait<WAIT_TIME> mWait;

  public:
    void init() FL_NO_EXCEPT override { FastPin<DATA_PIN>::setOutput(); }

    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }

  protected:
    void showPixels(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT override {
        mWait.wait();
        showRGBBlocking(pixels);
        mWait.mark();
    }

  private:
    void showRGBBlocking(PixelController<RGB_ORDER> pixels) FL_NO_EXCEPT {
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

        typedef FastPin<DATA_PIN> pin;
        volatile u32 *portBase = &sio_hw->gpio_out;
        const int portSetOff = (u32)&sio_hw->gpio_set - (u32)&sio_hw->gpio_out;
        const int portClrOff = (u32)&sio_hw->gpio_clr - (u32)&sio_hw->gpio_out;

        cli();
        showLedData<portSetOff, portClrOff, TIMING, RGB_ORDER, WAIT_TIME>(portBase, pin::mask(), pixels.mData, pixels.mLen, &data);
        sei();
    }
};

#endif  // FASTLED_RP2040_CLOCKLESS_PIO

}  // namespace fl
#endif // __INC_CLOCKLESS_RP_PIO_COMMON
