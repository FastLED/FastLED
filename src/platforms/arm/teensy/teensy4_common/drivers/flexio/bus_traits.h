#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::FLEX_IO> specialization for Teensy 4.x FlexIO2 driver.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.h"

namespace fl {

template<> struct BusTraits<Bus::FLEX_IO> {
    using Driver = ChannelEngineFlexIO;

    static Driver& instance() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return *gHolder;
    }
};

template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
