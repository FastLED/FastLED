#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/i2s_bus_traits.h
/// @brief BusTraits<Bus::FLEX_IO> specialization for the ESP8266 I2S engine.
///
/// Names the native I2S output engine (`Esp8266I2sChannelDriver`) for the
/// ESP8266 "I2S" bus, keyed on `Bus::FLEX_IO` — the same portable slot ESP32
/// uses for its I2S/PARLIO bank, so `busDriverName()` reports "I2S" here too.
/// Naming `BusTraits<Bus::FLEX_IO>::instance()` keeps the driver's translation
/// unit; otherwise it is dead-stripped (#2428), which is the template selection
/// the tracking issue asks for — no enable define (#4943 / #4949).
///
/// The I2S engine is exposed as an explicit bus; the ESP8266 clockless default
/// stays `Bus::BIT_BANG` (#4941), so a sketch that does not name `Bus::FLEX_IO`
/// is unaffected.

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
#include "platforms/esp/8266/i2s_channel_driver.h"

namespace fl {

template<> struct BusTraits<Bus::FLEX_IO> {
    using Driver = Esp8266I2sChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::registry().addDriver(default_bus_priority(Bus::FLEX_IO, 0),
                                             instancePtr());
    }
};

template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP8266
