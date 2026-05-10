#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::UART> specialization for the ESP32 UART wave8 driver.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_UART

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/uart/channel_driver_uart.h"
#include "platforms/esp/32/drivers/uart/uart_peripheral_esp.h"

namespace fl {

namespace detail {
/// @brief UART singleton: peripheral + driver lifetimes paired.
struct UartBusHolder {
    fl::shared_ptr<UartPeripheralEsp> peripheral;
    fl::shared_ptr<ChannelEngineUART> driver;
    UartBusHolder() FL_NOEXCEPT
        : peripheral(fl::make_shared<UartPeripheralEsp>()),
          driver(fl::make_shared<ChannelEngineUART>(peripheral)) {}
};
}  // namespace detail

template<> struct BusTraits<Bus::UART> {
    using Driver = ChannelEngineUART;

    static fl::shared_ptr<Driver> instancePtr() FL_NOEXCEPT {
        static detail::UartBusHolder gHolder;
        return gHolder.driver;
    }

    static Driver& instance() FL_NOEXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NOEXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::UART), instancePtr());
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_UART
