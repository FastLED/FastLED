#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::FLEX_IO, 0> for ESP32-S3 LCD_CAM.
///
/// Both buses share the LCD_CAM I80 peripheral on ESP32-S3 — LCD_SPI drives
/// true SPI chipsets (APA102 etc.) and LCD_CLOCKLESS drives clockless chipsets
/// (WS2812 etc.). They live in the same header because they share the same
/// `FASTLED_ESP32_HAS_LCD_SPI` feature flag and platform.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_LCD_SPI

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.h"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.h"

namespace fl {

// createLcdSpiEngine() and createLcdClocklessEngine() are declared in the
// driver headers included above.

namespace detail {
struct LcdCamBusHolder {
    fl::shared_ptr<IChannelDriver> spi;
    fl::shared_ptr<IChannelDriver> clockless;

    LcdCamBusHolder() FL_NO_EXCEPT
        : spi(createLcdSpiEngine()),
          clockless(createLcdClocklessEngine()) {}
};

inline detail::LcdCamBusHolder& lcd_cam_bus_holder() FL_NO_EXCEPT {
    static detail::LcdCamBusHolder gHolder;
    return gHolder;
}
}  // namespace detail

template<> struct BusTraits<Bus::FLEX_IO, 0> {
    using Driver = IChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        return detail::lcd_cam_bus_holder().clockless;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        auto& holder = detail::lcd_cam_bus_holder();
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 0), holder.spi);
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 0), holder.clockless);
    }
};

template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 0> : fl::true_type {};
template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset, 0> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_LCD_SPI
