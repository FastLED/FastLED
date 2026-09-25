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
#include "fl/channels/bus.h"
#include "fl/channels/slim_bridge_controller.h"
#include "platforms/arm/rp/rpcommon/rp_bitbang_bus_traits.h"
#endif

namespace fl {
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#if FASTLED_RP2040_CLOCKLESS_PIO

// Legacy `addLeds<>()` clockless controller for RP2040/RP2350. It is a slim
// bridge (issue #4589): the controller owns one ChannelData, re-encodes it
// every frame and enqueues it on the PIO channel engine
// (`BusTraits<Bus::FLEX_IO, 0>` == ChannelEngineRpPio on PIO0). Legacy strips
// and Channel API strips therefore share one PIO/DMA owner instead of two.
// XTRA0 travels on the ChannelData and the engine emits that many zero bits
// after every byte (GE8822 / GW6205). FLIP is accepted and ignored.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessRpPio
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::FLEX_IO, 0>, XTRA0> {
  public:
    ClocklessRpPio() FL_NO_EXCEPT = default;

    // Drive the data line LOW before the first frame: the engine only claims
    // the pin while transmitting, so without this the pad sits as an input
    // until the first show() (#4619 review).
    void init() FL_NO_EXCEPT override {
        FastPin<DATA_PIN>::lo();
        FastPin<DATA_PIN>::setOutput();
    }

    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessController = ClocklessRpPio<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

#else  // !FASTLED_RP2040_CLOCKLESS_PIO: M0 bit-bang via slim bridge

// Fallback when the PIO path is disabled: a slim bridge (issue #4635) onto
// the CPU bit-bang channel engine (`BusTraits<Bus::BIT_BANG, 0>` ==
// ChannelEngineRpBitBang). FLIP is accepted and ignored.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessRpBitBang
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::BIT_BANG, 0>, XTRA0> {
  public:
    ClocklessRpBitBang() FL_NO_EXCEPT = default;

    void init() FL_NO_EXCEPT override {
        FastPin<DATA_PIN>::lo();
        FastPin<DATA_PIN>::setOutput();
    }

    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessController = ClocklessRpBitBang<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

#endif  // FASTLED_RP2040_CLOCKLESS_PIO

}  // namespace fl
#endif // __INC_CLOCKLESS_RP_PIO_COMMON
