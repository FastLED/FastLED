#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::OBJECT_FLED> specialization for Teensy 4.x ObjectFLED.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.h"

namespace fl {

template<> struct BusTraits<Bus::OBJECT_FLED> {
    using Driver = ChannelEngineObjectFLED;

    static Driver& instance() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return *gHolder;
    }
};

template<> struct BusSupports<Bus::OBJECT_FLED, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
