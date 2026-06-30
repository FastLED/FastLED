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

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_RMT &&                           \
    !FASTLED_ESP32_RMT5_ONLY_PLATFORM && !FASTLED_RMT5

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_peripheral_esp.h"

namespace fl {

namespace detail {
/// @brief RMT4 singleton: peripheral + driver lifetimes paired (#3458).
///
/// Mirrors `UartBusHolder` — same shape so the channel-driver registry
/// is uniform across ESP32 peripherals. The peripheral is owned by this
/// holder; the engine holds a `shared_ptr` to it.
struct Rmt4BusHolder {
    fl::shared_ptr<Rmt4PeripheralESP> peripheral;
    fl::shared_ptr<ChannelEngineRMT4> driver;
    Rmt4BusHolder() FL_NO_EXCEPT
        : peripheral(fl::make_shared<Rmt4PeripheralESP>()),
          driver(ChannelEngineRMT4::create(peripheral)) {}
};
} // namespace detail

template <> struct BusTraits<Bus::RMT> {
    using Driver = ChannelEngineRMT4;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static detail::Rmt4BusHolder gHolder;
        return gHolder.driver;
    }

    static Driver &instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::RMT),
                                             instancePtr());
    }
};

template <> struct BusSupports<Bus::RMT, ClocklessChipset> : fl::true_type {};

} // namespace fl

#endif // FL_IS_ESP32 && FASTLED_ESP32_HAS_RMT && RMT4 path
