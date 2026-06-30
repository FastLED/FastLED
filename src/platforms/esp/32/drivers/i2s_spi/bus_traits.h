#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::FLEX_IO, 0> for ESP32-dev native I2S parallel SPI.
///
/// Drives true SPI chipsets (APA102, SK9822, HD108) via the original ESP32's
/// I2S parallel mode. ESP32-S3 uses the same portable `Bus::FLEX_IO` slot.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.h"

namespace fl {

// createI2sSpiEngine() is declared in channel_driver_i2s_spi.h (included above).

template<> struct BusTraits<Bus::FLEX_IO, 0> {
    using Driver = IChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = createI2sSpiEngine();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 0), instancePtr());
    }
};

template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 0> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_I2S
