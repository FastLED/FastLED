#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/uart_bus_traits.h
/// @brief BusTraits<Bus::UART> specialization for the ESP8266 UART1 driver.
///
/// Names the UART1 (GPIO2) WS2812 driver for `fl::Bus::UART`. Naming
/// `BusTraits<Bus::UART>::instance()` anywhere keeps the driver's translation
/// unit; if nothing references it the TU is dead-stripped (#2428). This is the
/// template selection that replaces the retired `FASTLED_ESP8266_UART`
/// code-enabling define (#4944).
///
/// UART1 TX is fixed to GPIO2, so this driver only claims a clockless channel
/// whose data pin is GPIO2. It is deliberately NOT registered by
/// `FastLED.enableAllDrivers()`: selecting it must be explicit
/// (`FastLED.add<Bus::UART, ClocklessChipset>(...)` or
/// `fl::enableDriver<Bus::UART>()`), so an ordinary pin-2 clockless strip keeps
/// going through the shared bit-bang driver.

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/8266/uart_channel_driver.h"

namespace fl {

template<> struct BusTraits<Bus::UART> {
    using Driver = Esp8266UartChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::registry().addDriver(default_bus_priority(Bus::UART, 0),
                                             instancePtr());
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP8266
