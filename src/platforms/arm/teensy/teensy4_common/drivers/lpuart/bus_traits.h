#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::LPUART> specialization for Teensy 4.x.

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

// Per user feedback: Bus::UART is the cross-platform clockless-via-UART
// bus name. On Teensy 4.x we bind it to the LPUART hardware via
// ChannelEngineLPUART -- same way the ESP32 binds Bus::UART to its
// own UART implementation. A sketch that says `Bus::UART` therefore
// works portably across ESP32 + Teensy 4.x.
//
// Bus::LPUART is kept as an alias that resolves to the same engine
// instance, so existing code that names the hardware directly still
// works -- but Bus::UART is the recommended name for portable
// sketches.
//
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

template<> struct BusTraits<Bus::LPUART> {
    using Driver = ChannelEngineLPUART;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        return BusTraits<Bus::UART>::instancePtr();  // same singleton
    }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::LPUART), instancePtr());
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};
template<> struct BusSupports<Bus::LPUART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
