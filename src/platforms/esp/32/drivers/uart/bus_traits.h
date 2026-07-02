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
/// One holder per UART block (FastLED#3576 Phase 2): uart_num 1 =
/// primary lane ("UART"), uart_num 2 = second lane ("UART2"). UART0 is
/// the console and is never used.
struct UartBusHolder {
    fl::shared_ptr<UartPeripheralEsp> peripheral;
    fl::shared_ptr<ChannelEngineUART> driver;
    explicit UartBusHolder(int uart_num) FL_NO_EXCEPT
        : peripheral(fl::make_shared<UartPeripheralEsp>()),
          driver(fl::make_shared<ChannelEngineUART>(peripheral, uart_num)) {}
};
}  // namespace detail

template<> struct BusTraits<Bus::UART> {
    using Driver = ChannelEngineUART;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static detail::UartBusHolder gHolder(/*uart_num=*/1);
        return gHolder.driver;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::UART, 0), instancePtr());
    }
};

// FastLED#3576 Phase 2 — second UART lane on UART2 ("UART2",
// `Bus::UART` instance 1). Classic ESP32 has three UARTs: UART0 is the
// console (off-limits), UART1 is the primary lane, UART2 is this one.
template<> struct BusTraits<Bus::UART, 1> {
    using Driver = ChannelEngineUART;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static detail::UartBusHolder gHolder(/*uart_num=*/2);
        return gHolder.driver;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::UART, 1), instancePtr());
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_UART
