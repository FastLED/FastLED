#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

/// @file platforms/arm/teensy/teensy4_common/clockless.h
/// @brief Teensy 4.0/4.1 platform-specific clockless controller dispatch
///
/// This header selects the appropriate clockless LED controller implementation
/// for Teensy 4.0 and 4.1 (IMXRT1062 platform).
///
/// Default: slim bridge onto ChannelEngineObjectFLED (parallel output, up to 42 strips)
/// Fallback: ClocklessController_BitBang (clockless_arm_mxrt1062.h) is a backward-compatible
/// alias onto the same ObjectFLED slim bridge (#4588); there is no bit-bang path.

#if defined(FL_IS_TEENSY_4X)  // Teensy 4.0/4.1

#include "fl/channels/bus.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/bus_traits.h"

namespace fl {

// Legacy `addLeds<>()` clockless controller for Teensy 4.x. It is a slim
// bridge (issue #4588): the controller owns one ChannelData, re-encodes it
// every frame and enqueues it on the ObjectFLED channel engine
// (`BusTraits<Bus::FLEX_IO, 0>` == ChannelEngineObjectFLED). ChannelManager
// owns the frame, so legacy strips and Channel API strips share one
// ObjectFLED back end instead of two independent DMA owners. Strips with the
// same timing and pixel format are still transmitted in parallel by the
// engine's timing groups.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessObjectFLED
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::FLEX_IO, 0>, XTRA0> {
  public:
    ClocklessObjectFLED() FL_NO_EXCEPT = default;

    u16 getMaxRefreshRate() const FL_NO_EXCEPT override {
        // Total bit period > 2000 ns is a 400 kHz chipset, otherwise 800 kHz.
        return (TIMING::T1 + TIMING::T2 + TIMING::T3) > 2000 ? 400 : 800;
    }
};

// Platform-default ClocklessController alias for Teensy 4.x.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessController = ClocklessObjectFLED<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

// Backward-compatible names for the removed ObjectFLED proxy.
template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER = RGB>
using ClocklessController_ObjectFLED_Proxy = ClocklessObjectFLED<DATA_PIN, TIMING, RGB_ORDER>;

template <int DATA_PIN, EOrder RGB_ORDER = RGB>
using ClocklessController_ObjectFLED_WS2812 = ClocklessObjectFLED<DATA_PIN, TIMING_WS2812_800KHZ, RGB_ORDER>;

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
