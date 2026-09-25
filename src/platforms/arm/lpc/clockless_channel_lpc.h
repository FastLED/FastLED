#pragma once

// IWYU pragma: private

/// @file clockless_channel_lpc.h
/// @brief LPC845 channels-API ClocklessController adapter
///
/// Routes the classic `addLeds<WS2812, PIN, GRB>()` API through the LPC845
/// SCT+DMA channels-API engine (`ChannelEngineLpcSctDma`). Meta issue #3517
/// Phase A.2; slimmed onto `fl::SlimBridgeController` under #4586.
///
/// `ClocklessController<>` is now a thin subclass of
/// `fl::SlimBridgeController` parameterized on
/// `BusTraits<Bus::BIT_BANG>` (`drivers/sct_dma/bus_traits.h`). The
/// constructor registers the driver with `ChannelManager`
/// (`DriverTraits::registerWithManager()`, idempotent), and every
/// `showPixels()` call re-encodes into the same `ChannelData` buffer and
/// enqueues it with the driver. There is no per-frame `driver->show()`
/// call anymore -- `ChannelManager`'s end-of-show flush drains all
/// registered drivers once per `FastLED.show()`.
///
/// Follows the exact pattern established by
/// `src/platforms/wasm/clockless_channel_wasm.h` and
/// `src/platforms/stub/clockless_channel_stub.h`: define
/// `FL_CLOCKLESS_CONTROLLER_DEFINED` so the fallback template
/// selection in `fastled_arm_lpc.h` compiles out; expose a
/// `ClocklessController<>` template that enqueues into the channels
/// engine on every `showPixels()`.
///
/// Only compiles when the user opts into the SCT+DMA path with
/// `-DFASTLED_LPC_PWM_DMA=1`. Without that flag `fastled_arm_lpc.h`
/// still picks the bit-bang default from `clockless_arm_lpc.h`.
///
/// On host/stub builds the engine's transmitter is a no-op (see
/// `lpc_sct_dma_runtime.cpp.hpp` host-mode block), so `addLeds<>` +
/// `FastLED.show()` compile and run without silicon. Real silicon
/// verification lands under Phase A.1 of #3517.

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Only take over the ClocklessController template when the user has opted
// into the SCT+DMA fast path. Without FASTLED_LPC_PWM_DMA the sketch keeps
// using the bit-bang controller from clockless_arm_lpc.h.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_LPC_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/slim_bridge_controller.h"
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"

namespace fl {

/// @brief LPC845 channels-API `ClocklessController` — routes addLeds<> to
///        the SCT+DMA engine via `BusTraits<Bus::BIT_BANG>`.
///
/// Template parameters mirror the classic `ClocklessController<>` template
/// so `addLeds<WS2812, PIN, GRB>()` binds without any user-facing change.
/// The SCT+DMA engine handles the actual byte-to-wire timing.
template <int DATA_PIN,
          typename TIMING,
          EOrder RGB_ORDER = RGB,
          int XTRA0 = 0,
          bool FLIP = false,
          int WAIT_TIME = 0>
class ClocklessController
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME,
                                   BusTraits<Bus::BIT_BANG>> {
public:
    u16 getMaxRefreshRate() const override { return 400; }
};

// Adapter for timing-like objects via duck typing. Matches the WASM /
// stub adapter signature so external code that instantiates it
// (e.g. FastLED chipset wrappers) still compiles.
template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter
    : public ClocklessController<DATA_PIN, TIMING_LIKE, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

// ClocklessBlockController for type-based timing (same behavior — the
// engine handles multi-block internally).
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController
    : public ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
