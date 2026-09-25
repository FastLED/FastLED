#pragma once

// IWYU pragma: private

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/singleton.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_bitbang.h"

namespace fl {
namespace detail {

struct RpBitBangBusHolder {
    fl::shared_ptr<ChannelEngineRpBitBang> driver;
    RpBitBangBusHolder() FL_NO_EXCEPT
        : driver(fl::make_shared<ChannelEngineRpBitBang>(
              createRpBitBangDevicePin(), rpBitBangCpuHz(), "BIT_BANG")) {}
};

inline fl::shared_ptr<ChannelEngineRpBitBang> rpBitBangInstancePtr() FL_NO_EXCEPT {
    return SingletonShared<RpBitBangBusHolder>::instance().driver;
}

}  // namespace detail

template<> struct BusTraits<Bus::BIT_BANG, 0> {
    using Driver = ChannelEngineRpBitBang;
    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT { return detail::rpBitBangInstancePtr(); }
    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }
    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::registry().addDriver(default_bus_priority(Bus::BIT_BANG, 0), instancePtr());
    }
};

template<> struct BusSupports<Bus::BIT_BANG, ClocklessChipset, 0> : fl::true_type {};

}  // namespace fl

#endif
