#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::BIT_BANG> specialization for the portable bit-bang driver.
///
/// `BitBangChannelDriver` is the universal fallback — any platform with GPIO
/// can drive both clockless and SPI LED strips by bit-banging. Supports up to
/// 8 parallel pins via `DigitalMultiWrite8`.

#include "fl/stl/compiler_control.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/shared/bitbang/bitbang_channel_driver.h"

namespace fl {

template<> struct BusTraits<Bus::BIT_BANG> {
    using Driver = BitBangChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NOEXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NOEXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::BIT_BANG), instancePtr());
    }
};

template<> struct BusSupports<Bus::BIT_BANG, ClocklessChipset> : fl::true_type {};
template<> struct BusSupports<Bus::BIT_BANG, SpiChipsetConfig>  : fl::true_type {};

}  // namespace fl
