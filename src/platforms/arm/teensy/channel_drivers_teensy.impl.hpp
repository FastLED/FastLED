#pragma once

// IWYU pragma: private

/// @file channel_drivers_teensy.impl.hpp
/// @brief Teensy channel-driver fragment for `fl::enableAllDrivers()`.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/arm/teensy/is_teensy.h"
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers // IWYU pragma: keep

#if defined(FL_IS_TEENSY_4X)
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/bus_traits.h"      // ok platform headers // IWYU pragma: keep
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/bus_traits.h"      // ok platform headers // IWYU pragma: keep
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/arm/teensy/teensy4_common/init_channel_driver.h"  // ok platform headers // IWYU pragma: keep
#endif

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
    fl::enableDrivers<
        fl::Bus::BIT_BANG
#if defined(FL_IS_TEENSY_4X)
        , fl::Bus::FLEX_IO
        , fl::Bus::UART
#endif
    >();
#if defined(FL_IS_TEENSY_4X)
    fl::enableDriver<fl::Bus::FLEX_IO, 1>();
    // FlexIO at priority 8 and the unified SPI adapter used to register on
    // first ChannelManager::instance() access (#4708).
    registerAllTeensyChannelDrivers();
#endif
}

}  // namespace platforms
}  // namespace fl
