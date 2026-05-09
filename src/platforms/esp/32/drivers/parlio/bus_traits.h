#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::PARLIO> specialization for ESP32-P4/C6/H2/C5.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_PARLIO

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.h"

namespace fl {

template<> struct BusTraits<Bus::PARLIO> {
    using Driver = ChannelDriverPARLIO;

    static Driver& instance() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return *gHolder;
    }
};

template<> struct BusSupports<Bus::PARLIO, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_PARLIO
