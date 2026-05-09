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
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/uart/channel_driver_uart.h"
#include "platforms/esp/32/drivers/uart/uart_peripheral_esp.h"

namespace fl {

template<> struct BusTraits<Bus::UART> {
    using Driver = ChannelEngineUART;

    /// @brief Constructs the UART peripheral and driver lazily on first call.
    /// Holding both shared_ptrs in a single struct keeps their lifetimes paired.
    static Driver& instance() FL_NOEXCEPT {
        struct Holder {
            fl::shared_ptr<UartPeripheralEsp> peripheral;
            fl::shared_ptr<Driver> driver;
            Holder() FL_NOEXCEPT
                : peripheral(fl::make_shared<UartPeripheralEsp>()),
                  driver(fl::make_shared<Driver>(peripheral)) {}
        };
        static Holder gHolder;
        return *gHolder.driver;
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_UART
