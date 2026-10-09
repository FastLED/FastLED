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
/// Factory for the LCD_CAM clockless driver. Null until a clockless channel
/// is created (platforms::enableClocklessEncoders() -> enableLcdClockless()),
/// so SPI-only programs never name createLcdClocklessEngine() and the
/// clockless pipeline is dropped at link time (#4793).
using LcdClocklessFactory = fl::shared_ptr<IChannelDriver> (*)();

inline LcdClocklessFactory& lcd_clockless_factory() FL_NO_EXCEPT {
    static LcdClocklessFactory gFactory = nullptr;
    return gFactory;
}

inline void enableLcdClockless() FL_NO_EXCEPT {
    lcd_clockless_factory() = &createLcdClocklessEngine;
}

struct LcdCamBusHolder {
    fl::shared_ptr<IChannelDriver> spi;
    fl::shared_ptr<IChannelDriver> clockless;

    LcdCamBusHolder() FL_NO_EXCEPT : spi(createLcdSpiEngine()) {}

    /// Lazily create the clockless driver once it has been enabled.
    const fl::shared_ptr<IChannelDriver>& clocklessPtr() FL_NO_EXCEPT {
        if (!clockless && lcd_clockless_factory()) {
            clockless = lcd_clockless_factory()();
        }
        return clockless;
    }
};

inline detail::LcdCamBusHolder& lcd_cam_bus_holder() FL_NO_EXCEPT {
    static detail::LcdCamBusHolder gHolder;
    return gHolder;
}
}  // namespace detail

template<> struct BusTraits<Bus::FLEX_IO, 0> {
    using Driver = IChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        auto& holder = detail::lcd_cam_bus_holder();
        const auto& clockless = holder.clocklessPtr();
        return clockless ? clockless : holder.spi;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    /// Clockless channels may route here: create and register the LCD_CAM
    /// clockless driver on demand (#4793). Registering here, not only in
    /// registerWithManager(), keeps it independent of construction order
    /// (an SPI controller may have registered this bus first).
    static void enableClockless() FL_NO_EXCEPT {
        detail::enableLcdClockless();
        if (const auto& clockless = detail::lcd_cam_bus_holder().clocklessPtr()) {
            ChannelManager::registry().addDriver(default_bus_priority(Bus::FLEX_IO, 0), clockless);
        }
    }

    static void registerWithManager() FL_NO_EXCEPT {
        auto& holder = detail::lcd_cam_bus_holder();
        ChannelManager::registry().addDriver(default_bus_priority(Bus::FLEX_IO, 0), holder.spi);
        if (const auto& clockless = holder.clocklessPtr()) {
            ChannelManager::registry().addDriver(default_bus_priority(Bus::FLEX_IO, 0), clockless);
        }
    }
};

template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 0> : fl::true_type {};
template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset, 0> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_LCD_SPI
