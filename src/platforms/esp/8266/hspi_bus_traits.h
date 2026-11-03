#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/hspi_bus_traits.h
/// @brief BusTraits<Bus::SPI> specialization for the ESP8266 hardware HSPI driver.
///
/// Names the register-level HSPI driver (`Esp8266HspiChannelDriver`, SPI1 on
/// GPIO13/14) for `fl::Bus::SPI`. Naming `BusTraits<Bus::SPI>::instance()`
/// anywhere keeps the driver's translation unit; if nothing references it the
/// TU is dead-stripped (#2428). This is the template selection that replaces
/// the retired `FASTLED_ALL_PINS_HARDWARE_SPI` code-enabling define (#4942).
///
/// The generic software bit-bang (`Bus::BIT_BANG`) still covers every other
/// pin pair, so only the fixed HSPI pair routes here.

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
#include "platforms/esp/8266/hspi_channel_driver.h"

namespace fl {

template<> struct BusTraits<Bus::SPI> {
    using Driver = Esp8266HspiChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::registry().addDriver(default_bus_priority(Bus::SPI, 0),
                                             instancePtr());
    }
};

template<> struct BusSupports<Bus::SPI, SpiChipsetConfig> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP8266
