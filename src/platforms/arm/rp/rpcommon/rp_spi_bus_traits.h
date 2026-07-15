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
#include "platforms/arm/rp/rpcommon/channel_engine_rp_spi.h"
#include "platforms/arm/rp/rpcommon/rp_spi_peripheral.h"

namespace fl {
namespace detail {

template<u8 Which>
struct RpSpiBusHolder {
    fl::shared_ptr<RpSpiPeripheral> peripheral;
    fl::shared_ptr<ChannelEngineRpSpi> driver;
    RpSpiBusHolder() FL_NO_EXCEPT
        : peripheral(fl::make_shared<RpSpiPeripheral>()),
          driver(fl::make_shared<ChannelEngineRpSpi>(peripheral, Which)) {}
};

template<u8 Which>
inline fl::shared_ptr<ChannelEngineRpSpi> rpSpiInstancePtr() FL_NO_EXCEPT {
    static RpSpiBusHolder<Which> holder;
    return holder.driver;
}

}  // namespace detail

template<> struct BusTraits<Bus::SPI, 0> {
    using Driver = ChannelEngineRpSpi;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT { return detail::rpSpiInstancePtr<0>(); }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::SPI, 0), instancePtr());
    }
};

template<> struct BusTraits<Bus::SPI, 1> {
    using Driver = ChannelEngineRpSpi;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT { return detail::rpSpiInstancePtr<1>(); }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::SPI, 1), instancePtr());
    }
};

template<> struct BusSupports<Bus::SPI, SpiChipsetConfig, 0> : fl::true_type {};
template<> struct BusSupports<Bus::SPI, SpiChipsetConfig, 1> : fl::true_type {};

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
