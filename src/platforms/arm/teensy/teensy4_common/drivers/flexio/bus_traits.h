#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::FLEX_IO, 1> specialization for Teensy 4.x FlexIO2 driver.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.h"

namespace fl {

template<> struct BusTraits<Bus::FLEX_IO, 1> {
    using Driver = ChannelEngineFlexIO;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 1), instancePtr());
    }
};

template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset, 1> : fl::true_type {};

// #3428: unified clockless + SPI engine -- the same FlexIO2 peripheral
// (and the same `ChannelEngineFlexIO` class) serves both modes. See
// `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals -- one
// engine for both clockless and SPI modes". Runtime support for SPI is
// gated by `flexio_spi_lookup_pins()`, which currently routes the
// APA102 default (MOSI=11/SCLK=13) and the Teensy 4.1 FlexIO2 SPI pin
// pairs covered by the flexioSpiSelfTest smoke test (3/3 pass at 1/6/
// 12 MHz on real hardware -- see PR #3431). This compile-time slot lets
// users write `FastLED.add<Bus::FLEX_IO, SpiChipsetConfig>(cfg)` and
// have it both type-check AND run.
template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 1> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
