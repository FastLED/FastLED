#pragma once

// IWYU pragma: private

/// @file clockless_channel_lpc_uart_dma.h
/// @brief LPC845 channels-API ClocklessController adapter for UART DMA.
///
/// Thin subclass of `fl::SlimBridgeController` (see
/// `fl/channels/slim_bridge_controller.h`) over `BusTraits<Bus::UART>`.
/// Flushing no longer happens per-frame in `showPixels()`: the controller
/// only enqueues into its `ChannelData`, and `ChannelManager`'s end-of-show
/// path is responsible for driving the actual flush. `SlimBridgeController`
/// itself sets the RGB/RGBW/RGBWW pixel format on the encoded `ChannelData`
/// based on the caller's `getRgbw()`/`getRgbww()` state.

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC_845) && FASTLED_LPC_UART_DMA

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_LPC_UART_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/arm/lpc/drivers/uart_dma/bus_traits.h"

namespace fl {

FL_STATIC_ASSERT(DefaultBus<ClocklessChipset>::value == Bus::UART,
                 "FASTLED_LPC_UART_DMA must make LPC845 clockless AUTO resolve to Bus::UART");

template <int DATA_PIN,
          typename TIMING,
          EOrder RGB_ORDER = RGB,
          int XTRA0 = 0,
          bool FLIP = false,
          int WAIT_TIME = 0>
class ClocklessController
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::UART>> {
public:
    u16 getMaxRefreshRate() const override { return 400; }
};

template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter
    : public ClocklessController<DATA_PIN, TIMING_LIKE, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController
    : public ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA
