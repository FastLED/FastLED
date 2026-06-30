#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::SPI> specialization for the clockless-over-SPI driver.
///
/// Despite the bus name, this driver implements clockless protocols (WS2812,
/// SK6812, etc.) using SPI hardware as a precise bit-pulse generator. It does
/// NOT drive true SPI chipsets like APA102 (those go through the platform's
/// selected true-SPI path, often `Bus::FLEX_IO` slot 0 on ESP32-dev/S3).

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/spi/channel_driver_spi.h"

namespace fl {

template<> struct BusTraits<Bus::SPI> {
    using Driver = ChannelEngineSpi;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::SPI), instancePtr());
    }
};

template<> struct BusSupports<Bus::SPI, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_CLOCKLESS_SPI
