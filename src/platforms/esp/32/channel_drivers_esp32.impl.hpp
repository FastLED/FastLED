#pragma once

// IWYU pragma: private

/// @file channel_drivers_esp32.impl.hpp
/// @brief ESP32 channel-driver fragment for `fl::enableAllDrivers()`.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers // IWYU pragma: keep

// Each per-driver bus_traits.h applies its own `FASTLED_ESP32_HAS_*` /
// `FASTLED_RMT5` guard internally, so the include is unconditional here.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"        // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/i2s_spi/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/parlio/bus_traits.h"     // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/rmt/rmt_4/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/spi/bus_traits.h"        // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/uart/bus_traits.h"       // ok platform headers // IWYU pragma: keep

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
    fl::enableDrivers<
        fl::Bus::BIT_BANG
#if FASTLED_ESP32_HAS_RMT
        , fl::Bus::RMT
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
        , fl::Bus::SPI
#endif
#if FASTLED_ESP32_HAS_PARLIO || FASTLED_ESP32_HAS_I2S || FASTLED_ESP32_HAS_LCD_SPI
        , fl::Bus::FLEX_IO
#endif
#if FASTLED_ESP32_HAS_UART
        , fl::Bus::UART
#endif
    >();
#if FASTLED_ESP32_HAS_LCD_RGB
    fl::enableDriver<fl::Bus::FLEX_IO, 1>();
#endif
}

}  // namespace platforms
}  // namespace fl
