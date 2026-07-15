#pragma once

// IWYU pragma: private

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_uart.h"
#include "platforms/arm/rp/rpcommon/rp_uart_peripheral.h"

namespace fl {
namespace detail {

struct RpUartBusHolder {
    fl::shared_ptr<RpUartPeripheral> peripheral;
    fl::shared_ptr<ChannelEngineRpUart> driver;
    explicit RpUartBusHolder(u8 uart_index) FL_NO_EXCEPT
        : peripheral(fl::make_shared<RpUartPeripheral>()),
          driver(fl::make_shared<ChannelEngineRpUart>(peripheral, uart_index)) {}
};

template<u8 UartIndex>
inline fl::shared_ptr<ChannelEngineRpUart> rpUartInstancePtr() FL_NO_EXCEPT {
    static RpUartBusHolder holder(UartIndex);
    return holder.driver;
}

}  // namespace detail

template<> struct BusTraits<Bus::UART, 0> {
    using Driver = ChannelEngineRpUart;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        return detail::rpUartInstancePtr<0>();
    }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::UART, 0), instancePtr());
    }
};

template<> struct BusTraits<Bus::UART, 1> {
    using Driver = ChannelEngineRpUart;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        return detail::rpUartInstancePtr<1>();
    }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::UART, 1), instancePtr());
    }
};

template<> struct BusSupports<Bus::UART, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
