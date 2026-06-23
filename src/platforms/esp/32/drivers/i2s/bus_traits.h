#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::I2S> specialization for ESP32-S3 LCD_CAM (I2S mode).
///
/// Confusing legacy name: this driver uses the LCD_CAM peripheral in I2S mode
/// to drive clockless LED strips on ESP32-S3. The "I2S" name is retained here
/// for backward compatibility — the actual modern S3 clockless LCD_CAM driver
/// lives at `Bus::LCD_CLOCKLESS`.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S_LCD_CAM

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/i2s/channel_driver_i2s.h"

namespace fl {

// createI2sEngine() is declared in channel_driver_i2s.h (included above).

template<> struct BusTraits<Bus::I2S> {
    using Driver = IChannelDriver;  // factory returns abstract base

    static fl::shared_ptr<Driver> instancePtr() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = createI2sEngine();
        return gHolder;
    }

    static Driver& instance() FL_NOEXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NOEXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::I2S), instancePtr());
    }
};

template<> struct BusSupports<Bus::I2S, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_I2S_LCD_CAM
