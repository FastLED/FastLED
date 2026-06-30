#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::BIT_BANG> specialization for the LPC845 SCT+DMA engine.
///
/// Routes `Bus::BIT_BANG` on LPC845 to `ChannelEngineLpcSctDma`. The portable
/// Bus enum has no `SCT_DMA` slot — and per the channels-API convention the
/// **peripheral** is the dispatch boundary, not the protocol mode — so we
/// reuse `BIT_BANG` for the parallel-IO clockless peripheral on LPC. This is
/// the same Bus value that `DefaultBus<ClocklessChipset>` resolves to on
/// LPC (added in PR #3451 / #3450 B4).
///
/// Bus naming follows precedent: `Bus::FLEX_IO` covers both Teensy 4 FlexPWM
/// and ESP32 LCD_CAM / I2S / PARLIO peripherals — different silicon under
/// the same Bus slot, dispatched by `Which` + platform gate.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ARM_LPC_845)

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h"

namespace fl {

template<> struct BusTraits<Bus::BIT_BANG> {
    using Driver = ChannelEngineLpcSctDma;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(
            default_bus_priority(Bus::BIT_BANG, 0), instancePtr());
    }
};

// Clockless-only: the LPC SCT peripheral does not drive SPI; the LPC845 SPI
// block is a separate peripheral with its own DMA engine
// (`spi_arm_lpc_dma.h`, PR #3454). Per the channels-API parallel-IO rule
// this is the documented "different-peripheral-same-protocol" exception.
template<> struct BusSupports<Bus::BIT_BANG, ClocklessChipset, 0> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845
