#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::UART> specialization for Teensy 4.x LPUART.

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
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/channel_engine_lpuart.h"

namespace fl {

// CodeRabbit-flagged: function-local static with non-trivial ctor is
// disallowed in `src/**/*.h`. instancePtr() is defined in
// channel_engine_lpuart.cpp.hpp so the static storage lives in a TU,
// not in this header.
template<> struct BusTraits<Bus::UART> {
    using Driver = ChannelEngineLPUART;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT;
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::UART), instancePtr());
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
