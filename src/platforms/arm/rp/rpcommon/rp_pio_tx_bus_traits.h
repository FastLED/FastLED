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
#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.h"
#include "platforms/arm/rp/rpcommon/rp_pio_spi_peripheral.h"
#include "platforms/arm/rp/rpcommon/rp_pio_tx_peripheral.h"

namespace fl {
namespace detail {

template<u8 Which>
struct RpPioTxBusHolder {
    fl::shared_ptr<RpPioTxPeripheral> peripheral;
    fl::shared_ptr<RpPioSpiPeripheral> spiPeripheral;
    fl::shared_ptr<ChannelEngineRpPio> driver;
    RpPioTxBusHolder() FL_NO_EXCEPT
        : peripheral(fl::make_shared<RpPioTxPeripheral>(Which)),
          spiPeripheral(fl::make_shared<RpPioSpiPeripheral>(Which)),
          driver(fl::make_shared<ChannelEngineRpPio>(peripheral, spiPeripheral)) {}
};

template<u8 Which>
inline fl::shared_ptr<ChannelEngineRpPio> rpPioTxInstancePtr() FL_NO_EXCEPT {
    static RpPioTxBusHolder<Which> holder;
    return holder.driver;
}

}  // namespace detail

template<> struct BusTraits<Bus::FLEX_IO, 0> {
    using Driver = ChannelEngineRpPio;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT { return detail::rpPioTxInstancePtr<0>(); }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 0), instancePtr());
    }
};

template<> struct BusTraits<Bus::FLEX_IO, 1> {
    using Driver = ChannelEngineRpPio;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT { return detail::rpPioTxInstancePtr<1>(); }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 1), instancePtr());
    }
};

template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset, 0> : fl::true_type {};
template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset, 1> : fl::true_type {};
template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 0> : fl::true_type {};
template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 1> : fl::true_type {};

}  // namespace fl

#endif
