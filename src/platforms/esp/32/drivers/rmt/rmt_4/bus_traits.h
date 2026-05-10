#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::RMT> specialization for the ESP-IDF 4.x RMT4 driver.
///
/// Mutually exclusive with the RMT5 specialization in
/// `platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h` — the platform's
/// `FASTLED_RMT5` flag determines which TU compiles, so only one definition
/// is ever produced for `BusTraits<Bus::RMT>`.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_RMT && !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.h"

namespace fl {

template<> struct BusTraits<Bus::RMT> {
    using Driver = ChannelEngineRMT4;

    static fl::shared_ptr<Driver> instancePtr() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = ChannelEngineRMT4::create();
        return gHolder;
    }

    static Driver& instance() FL_NOEXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NOEXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::RMT), instancePtr());
    }
};

template<> struct BusSupports<Bus::RMT, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_RMT && RMT4 path
